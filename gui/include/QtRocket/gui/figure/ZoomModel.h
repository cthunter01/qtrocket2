#pragma once

#include <QObject>

namespace QtRocket::Gui
{

/// What a ScaleSelector zooms (OpenRocket's ScaleSelector.ZoomModel): a scale, 1 being 100 %, and
/// whether it is fitted. ScrollAreaZoomModel is the one of a ScaleScrollArea (the user scale in
/// the fin point editor, the scale relative to the fit in the design view); the 3D view
/// (Milestone 6) brings its own. changed() replaces Java's change listeners.
class ZoomModel : public QObject
{
    Q_OBJECT

public:
    explicit ZoomModel(QObject* parent);

    /// The scale shown (getScale()); it may bring the figure behind it up to date first, so it is
    /// not const.
    [[nodiscard]] virtual double scale() = 0;
    /// Whether the scale is fitted (isFit()).
    [[nodiscard]] virtual bool isFit() const = 0;
    /// Asks for the scale @p scale (setScale()); a model may apply it later, and emits changed()
    /// when it does.
    virtual void setScale(double scale) = 0;
    /// Asks for the fitted scale (setFit()).
    virtual void setFit() = 0;

Q_SIGNALS:
    /// The scale or the fit changed.
    void changed();
};

}  // namespace QtRocket::Gui
