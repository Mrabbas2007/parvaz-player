#pragma once

#include <QObject>
#include <QString>
#include <mpv/client.h>

namespace parvaz {

class ShaderManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString shaderPreset READ shaderPreset WRITE setShaderPreset NOTIFY shaderPresetChanged)
    Q_PROPERTY(bool isBypassedFor4k READ isBypassedFor4k NOTIFY shaderPresetChanged)

public:
    explicit ShaderManager(mpv_handle *mpv, QObject *parent = nullptr);
    ~ShaderManager() override = default;

    QString shaderPreset() const { return m_preset; }
    void setShaderPreset(const QString &preset);

    bool isBypassedFor4k() const { return m_isBypassedFor4k; }

    void onVideoParamsChanged(int64_t width, int64_t height);
    void checkPerformanceGuard(int64_t currentDropCount);

signals:
    void shaderPresetChanged();
    void performanceWarning(const QString &message);

private:
    void ensureShadersReady();
    void applyCurrentShader();

    mpv_handle *m_mpv = nullptr;
    QString m_preset = QStringLiteral("off");
    QString m_shadersDir;
    int64_t m_videoWidth = 0;
    int64_t m_videoHeight = 0;
    bool m_isBypassedFor4k = false;

    // Circuit breaker state
    int64_t m_lastDropCount = 0;
    qint64 m_presetActivationTime = 0;
    bool m_circuitBreakerTriggered = false;
};

} // namespace parvaz
