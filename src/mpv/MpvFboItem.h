#pragma once

#include <QQuickFramebufferObject>

/// QML item that hosts libmpv's OpenGL render context.
///
/// Threading contract:
///   - synchronize() runs on the render thread with the GL context current.
///     Create/update the mpv render context there.
///   - render() runs on the render thread with the GL context current.
///   - Never touch mpv_handle from here.
/// Total frames pushed to the screen (for the smoke test / benchmark).
int parvazRenderedFrames();

class MpvFboItem : public QQuickFramebufferObject {
    Q_OBJECT

public:
    explicit MpvFboItem(QQuickItem *parent = nullptr);
    ~MpvFboItem() override;

    Renderer *createRenderer() const override;
};
