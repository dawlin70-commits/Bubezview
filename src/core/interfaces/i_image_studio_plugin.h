/**
 * @file i_image_studio_plugin.h
 * @brief Qt plugin interface for the modular Image Studio engine (bubez_image.so).
 *
 * Keeps advanced photo editing controls (color grading presets, light/color adjustments,
 * auto-enhance, sharpen/blur/vignette, and future studio tools) decoupled from the
 * lightweight BubezView core image viewer.
 */

#pragma once

#include <QtPlugin>
#include <QImage>
#include <QString>
#include <QWidget>
#include <functional>

/**
 * @class IImageStudioWidget
 * @brief Abstract studio panel interface implemented inside bubez_image.so.
 */
class IImageStudioWidget : public QWidget {
public:
    using QWidget::QWidget;
    ~IImageStudioWidget() override = default;

    /**
     * @brief Supplies the current base image to preview and edit.
     * @param image Source QImage from ImageViewer.
     */
    virtual void setSourceImage(const QImage &image) = 0;

    /**
     * @brief Synchronizes visual styling with the active BubezView theme preset.
     */
    virtual void updateTheme() = 0;

    /**
     * @brief Registers callback invoked whenever the studio produces an edited QImage.
     * @param cb Callback receiving the modified QImage.
     */
    virtual void setOnApplyImage(std::function<void(const QImage &)> cb) = 0;

    /**
     * @brief Registers callback invoked to trigger interactive crop mode in ImageViewer.
     * @param cb Callback functor.
     */
    virtual void setOnRequestCrop(std::function<void()> cb) = 0;

    /**
     * @brief Registers callback invoked to rotate the image 90 degrees.
     * @param cb Callback taking true for clockwise, false for counter-clockwise.
     */
    virtual void setOnRequestRotate(std::function<void(bool clockwise)> cb) = 0;

    /**
     * @brief Registers callback invoked to flip the image horizontally or vertically.
     * @param cb Callback taking true for horizontal, false for vertical.
     */
    virtual void setOnRequestFlip(std::function<void(bool horizontal)> cb) = 0;

    /**
     * @brief Registers callback invoked to flatten transparency onto white.
     * @param cb Callback functor.
     */
    virtual void setOnRequestRemoveTransparency(std::function<void()> cb) = 0;
};

/**
 * @class IImageStudioPlugin
 * @brief Qt plugin factory interface implemented by bubez_image.so.
 */
class IImageStudioPlugin {
public:
    virtual ~IImageStudioPlugin() = default;

    /**
     * @brief Unique identifier of the plugin ("image").
     * @return Plugin ID string.
     */
    virtual QString pluginId() const = 0;

    /**
     * @brief Human-readable plugin name.
     * @return Display name string.
     */
    virtual QString displayName() const = 0;

    /**
     * @brief Short description of plugin capabilities.
     * @return Description string.
     */
    virtual QString description() const = 0;

    /**
     * @brief Instantiates an embedded Image Studio panel widget.
     * @param parent Optional parent widget.
     * @return Newly created IImageStudioWidget instance.
     */
    virtual IImageStudioWidget *createStudioWidget(QWidget *parent = nullptr) = 0;
};

#define IImageStudioPlugin_iid "org.bubezview.IImageStudioPlugin/1.0"
Q_DECLARE_INTERFACE(IImageStudioPlugin, IImageStudioPlugin_iid)
