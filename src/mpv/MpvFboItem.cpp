#include "mpv/MpvFboItem.h"
#include "mpv/MpvObject.h"

#include <mpv/render_gl.h>   // mpv_render_* and mpv_opengl_* live here, not in client.h

#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPointer>
#include <QDebug>

#include <atomic>
#include <cstdio>

namespace {

std::atomic<int> g_renderedFrames{0};

void *mpvGetProcAddress(void *ctx, const char *name)
{
    Q_UNUSED(ctx)
    QOpenGLContext *gl = QOpenGLContext::currentContext();
    if (!gl)
        return nullptr;
    return reinterpret_cast<void *>(gl->getProcAddress(name));
}

} // namespace

int parvazRenderedFrames() { return g_renderedFrames.load(); }

class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(MpvFboItem *item) : m_item(item) {}

    ~MpvRenderer() override
    {
        // The render context must die before mpv_terminate_destroy().
        if (m_ctx) {
            mpv_render_context_free(m_ctx);
            m_ctx = nullptr;
        }
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        m_item = static_cast<MpvFboItem *>(item);

        // In Qt6 the FBO lives behind Renderer::framebufferObject(), and it is
        // only valid here — never stash it for use in render().
        if (QOpenGLFramebufferObject *fb = framebufferObject()) {
            m_fbo = fb->handle();
            m_w = fb->width();
            m_h = fb->height();
        }

        if (m_ctx)
            return;

        MpvObject *mpv = MpvObject::instance();
        if (!mpv || !mpv->handle()) {
            qWarning("parvaz: synchronize() before mpv handle exists");
            return;
        }

        // Positional init: newer client API has a third member (extra_exts),
        // which is zero-filled here on purpose.
        mpv_opengl_init_params glInit{&mpvGetProcAddress, nullptr};

        // NOTE: ADVANCED_CONTROL stays off. It requires calling
        // mpv_render_context_report_swap() for correct frame timing; without
        // it mpv renders one frame and then stalls (time-pos freezes at 0).
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE,
             const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        if (mpv_render_context_create(&m_ctx, mpv->handle(), params) < 0) {
            qWarning("parvaz: mpv_render_context_create failed");
            m_ctx = nullptr;
            return;
        }

        mpv_render_context_set_update_callback(m_ctx, &MpvRenderer::onMpvUpdate, this);

        // Hand the video output over to the GUI thread: playback may start now.
        QMetaObject::invokeMethod(mpv, [mpv]() { mpv->setRenderReady(); },
                                  Qt::QueuedConnection);
    }

    void render() override
    {
        if (!m_ctx)
            return;

        mpv_opengl_fbo fbo{m_fbo, m_w, m_h, 0};
        int flipY = 1; // if the image shows up upside down, set this to 0
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        mpv_render_context_render(m_ctx, params);
        ++g_renderedFrames;
    }

private:
    static void onMpvUpdate(void *ctx)
    {
        auto *self = static_cast<MpvRenderer *>(ctx);
        MpvFboItem *item = self->m_item.data();
        if (!item)
            return;

        QPointer<MpvFboItem> guard(item);
        QMetaObject::invokeMethod(item, [guard]() {
            if (guard)
                guard->update();
        }, Qt::QueuedConnection);
    }

    QPointer<MpvFboItem> m_item;
    mpv_render_context *m_ctx = nullptr;
    int m_fbo = 0;
    int m_w = 1;
    int m_h = 1;
};

MpvFboItem::MpvFboItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

MpvFboItem::~MpvFboItem() = default;

QQuickFramebufferObject::Renderer *MpvFboItem::createRenderer() const
{
    return new MpvRenderer(const_cast<MpvFboItem *>(this));
}
