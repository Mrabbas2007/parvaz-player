#include "playback/ShaderManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <cstdio>

namespace parvaz {

static const char *s_fsrGlsl = R"GLSL(//!HOOK MAIN
//!DESC AMD FidelityFX Super Resolution (RCAS)
//!BIND HOOKED

vec4 hook() {
    vec2 pos = HOOKED_pos;
    vec2 pt = HOOKED_pt;

    vec4 b = HOOKED_tex(pos + vec2(0.0, -pt.y));
    vec4 d = HOOKED_tex(pos + vec2(-pt.x, 0.0));
    vec4 e = HOOKED_tex(pos);
    vec4 f = HOOKED_tex(pos + vec2(pt.x, 0.0));
    vec4 h = HOOKED_tex(pos + vec2(0.0, pt.y));

    // Min / Max neighborhood luminance
    float b_lum = dot(b.rgb, vec3(0.2126, 0.7152, 0.0722));
    float d_lum = dot(d.rgb, vec3(0.2126, 0.7152, 0.0722));
    float e_lum = dot(e.rgb, vec3(0.2126, 0.7152, 0.0722));
    float f_lum = dot(f.rgb, vec3(0.2126, 0.7152, 0.0722));
    float h_lum = dot(h.rgb, vec3(0.2126, 0.7152, 0.0722));

    float min_lum = min(e_lum, min(min(b_lum, d_lum), min(f_lum, h_lum)));
    float max_lum = max(e_lum, max(max(b_lum, d_lum), max(f_lum, h_lum)));

    // FSR Contrast Adaptive Peak weight
    float sharpness = 0.25;
    float w = clamp(min(min_lum, 2.0 - max_lum) / (sharpness * max_lum + 1e-5), 0.0, 0.18);

    vec3 res = (b.rgb * w + d.rgb * w + f.rgb * w + h.rgb * w + e.rgb) / (1.0 + 4.0 * w);
    return vec4(clamp(res, 0.0, 1.0), e.a);
}
)GLSL";

static const char *s_anime4kLiteGlsl = R"GLSL(//!HOOK MAIN
//!DESC Anime4K Fast Line Thinning & Deblur
//!BIND HOOKED

vec4 hook() {
    vec2 pos = HOOKED_pos;
    vec2 pt = HOOKED_pt;

    vec4 c = HOOKED_tex(pos);
    vec4 l = HOOKED_tex(pos - vec2(pt.x, 0.0));
    vec4 r = HOOKED_tex(pos + vec2(pt.x, 0.0));
    vec4 u = HOOKED_tex(pos - vec2(0.0, pt.y));
    vec4 d = HOOKED_tex(pos + vec2(0.0, pt.y));

    float cl = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    float ll = dot(l.rgb, vec3(0.299, 0.587, 0.114));
    float rl = dot(r.rgb, vec3(0.299, 0.587, 0.114));
    float ul = dot(u.rgb, vec3(0.299, 0.587, 0.114));
    float dl = dot(d.rgb, vec3(0.299, 0.587, 0.114));

    float xgrad = (rl - ll) * 0.5;
    float ygrad = (dl - ul) * 0.5;
    float edge = clamp(sqrt(xgrad * xgrad + ygrad * ygrad) * 2.0, 0.0, 1.0);

    vec3 sharpened = mix(c.rgb, (c.rgb * 1.3 - 0.075 * (l.rgb + r.rgb + u.rgb + d.rgb)), edge * 0.45);
    return vec4(clamp(sharpened, 0.0, 1.0), c.a);
}
)GLSL";

static const char *s_anime4kHighGlsl = R"GLSL(//!HOOK MAIN
//!DESC Anime4K High Quality Edge Reconstruction
//!BIND HOOKED

vec4 hook() {
    vec2 pos = HOOKED_pos;
    vec2 pt = HOOKED_pt;

    vec4 c = HOOKED_tex(pos);
    vec4 l = HOOKED_tex(pos - vec2(pt.x, 0.0));
    vec4 r = HOOKED_tex(pos + vec2(pt.x, 0.0));
    vec4 u = HOOKED_tex(pos - vec2(0.0, pt.y));
    vec4 d = HOOKED_tex(pos + vec2(0.0, pt.y));
    vec4 lu = HOOKED_tex(pos - pt);
    vec4 ru = HOOKED_tex(pos + vec2(pt.x, -pt.y));
    vec4 ld = HOOKED_tex(pos + vec2(-pt.x, pt.y));
    vec4 rd = HOOKED_tex(pos + pt);

    float cl = dot(c.rgb, vec3(0.299, 0.587, 0.114));

    vec4 gx = -lu - 2.0 * l - ld + ru + 2.0 * r + rd;
    vec4 gy = -lu - 2.0 * u - ru + ld + 2.0 * d + rd;
    vec4 g = sqrt(gx * gx + gy * gy);
    float edge = clamp(dot(g.rgb, vec3(0.333)), 0.0, 1.0);

    vec3 laplace = (c.rgb * 8.0 - (l.rgb + r.rgb + u.rgb + d.rgb + lu.rgb + ru.rgb + ld.rgb + rd.rgb));
    vec3 refined = c.rgb + laplace * (edge * 0.25);
    return vec4(clamp(refined, 0.0, 1.0), c.a);
}
)GLSL";

ShaderManager::ShaderManager(mpv_handle *mpv, QObject *parent)
    : QObject(parent)
    , m_mpv(mpv)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    m_shadersDir = base + QStringLiteral("/shaders");
    QDir().mkpath(m_shadersDir);

    ensureShadersReady();
}

void ShaderManager::ensureShadersReady()
{
    auto writeIfMissing = [](const QString &path, const char *content) {
        if (!QFile::exists(path)) {
            QFile file(path);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(content);
                file.close();
            }
        }
    };

    writeIfMissing(m_shadersDir + QStringLiteral("/fsr.glsl"), s_fsrGlsl);
    writeIfMissing(m_shadersDir + QStringLiteral("/anime4k_lite.glsl"), s_anime4kLiteGlsl);
    writeIfMissing(m_shadersDir + QStringLiteral("/anime4k_high.glsl"), s_anime4kHighGlsl);
}

void ShaderManager::setShaderPreset(const QString &preset)
{
    const QString p = preset.toLower().trimmed();
    if (m_preset == p)
        return;

    m_preset = p;
    m_circuitBreakerTriggered = false;
    m_presetActivationTime = QDateTime::currentMSecsSinceEpoch();

    applyCurrentShader();
    emit shaderPresetChanged();
}

void ShaderManager::onVideoParamsChanged(int64_t width, int64_t height)
{
    m_videoWidth = width;
    m_videoHeight = height;

    // Rule: Never upscale native 4K media (w >= 2560 or h >= 1440) to prevent GPU waste.
    const bool shouldBypass = (m_videoWidth >= 2560 || m_videoHeight >= 1440);
    if (shouldBypass != m_isBypassedFor4k) {
        m_isBypassedFor4k = shouldBypass;
        applyCurrentShader();
        emit shaderPresetChanged();
    }
}

void ShaderManager::checkPerformanceGuard(int64_t currentDropCount)
{
    if (m_preset == QLatin1String("off") || m_circuitBreakerTriggered) {
        m_lastDropCount = currentDropCount;
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_presetActivationTime > 1500) { // Check after shader has had 1.5s to stabilize
        const int64_t newDrops = currentDropCount - m_lastDropCount;
        if (newDrops > 15) { // More than 15 frame drops detected in this interval
            m_circuitBreakerTriggered = true;
            if (m_preset == QLatin1String("anime4k-high")) {
                m_preset = QStringLiteral("fsr");
                emit performanceWarning(QStringLiteral("High GPU load: switched from Anime4K High to AMD FSR"));
            } else {
                m_preset = QStringLiteral("off");
                emit performanceWarning(QStringLiteral("High GPU load: disabled upscaling shaders to prevent lag"));
            }
            applyCurrentShader();
            emit shaderPresetChanged();
        }
    }
    m_lastDropCount = currentDropCount;
}

void ShaderManager::applyCurrentShader()
{
    if (!m_mpv)
        return;

    if (m_isBypassedFor4k || m_preset == QLatin1String("off") || m_preset.isEmpty()) {
        mpv_set_property_string(m_mpv, "glsl-shaders", "");
        return;
    }

    QString shaderFile;
    if (m_preset == QLatin1String("fsr")) {
        shaderFile = m_shadersDir + QStringLiteral("/fsr.glsl");
    } else if (m_preset == QLatin1String("anime4k-lite")) {
        shaderFile = m_shadersDir + QStringLiteral("/anime4k_lite.glsl");
    } else if (m_preset == QLatin1String("anime4k-high")) {
        shaderFile = m_shadersDir + QStringLiteral("/anime4k_high.glsl");
    }

    if (!shaderFile.isEmpty() && QFile::exists(shaderFile)) {
        mpv_set_property_string(m_mpv, "glsl-shaders", shaderFile.toUtf8().constData());
    } else {
        mpv_set_property_string(m_mpv, "glsl-shaders", "");
    }
}

} // namespace parvaz
