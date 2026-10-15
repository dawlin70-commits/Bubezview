/**
 * @file i_viewer_widget.h
 * @brief Base abstract interface for all modular viewer widgets in BubezView.
 *
 * Provides a unified contract for file loading, zooming, rotation, navigation,
 * and state management across all core and plugin viewers.
 */

#pragma once

#include <QString>
#include <QWidget>

/**
 * @class IViewerWidget
 * @brief Abstract interface that every viewer component implements.
 */
class IViewerWidget {
public:
    virtual ~IViewerWidget() = default;

    /**
     * @brief Returns the underlying QWidget instance for layout embedding.
     * @return Pointer to QWidget.
     */
    virtual QWidget *widget() = 0;

    /**
     * @brief Loads and renders a document or media file.
     * @param filePath Absolute path of the target file.
     * @return True if loaded successfully; false otherwise.
     */
    virtual bool loadFile(const QString &filePath) = 0;

    /**
     * @brief Clears current view, freeing allocated resources and document buffers.
     */
    virtual void clear() = 0;

    /**
     * @brief Refreshes colors, icons, and styling based on active ThemeManager skin.
     */
    virtual void updateTheme() {}

    /**
     * @brief Checks if document has unsaved modifications.
     * @return True if modified.
     */
    virtual bool isModified() const { return false; }

    /**
     * @brief Saves modifications back to original file.
     * @return True if successful.
     */
    virtual bool save() { return false; }

    /**
     * @brief Saves document to a new target path.
     * @param targetFilePath Destination path.
     * @return True if successful.
     */
    virtual bool saveAs(const QString &targetFilePath) { Q_UNUSED(targetFilePath); return false; }

    /**
     * @brief Checks if undo operation is available.
     * @return True if undo can be performed.
     */
    virtual bool canUndo() const { return false; }

    /**
     * @brief Performs undo on previous action.
     */
    virtual void undo() {}

    /**
     * @brief Checks if redo operation is available.
     * @return True if redo can be performed.
     */
    virtual bool canRedo() const { return false; }

    /**
     * @brief Performs redo on undone action.
     */
    virtual void redo() {}

    /**
     * @brief Pastes content from system clipboard into the active document.
     */
    virtual void paste() {}

    /**
     * @brief Searches forward for text in the viewer.
     * @param text Target search string.
     * @param matchCase Case sensitivity toggle.
     * @param wrapped Set to true if search wrapped around document boundary.
     * @return True if match found.
     */
    virtual bool findNext(const QString &text, bool matchCase, bool *wrapped = nullptr) {
        Q_UNUSED(text); Q_UNUSED(matchCase); if (wrapped) *wrapped = false; return false;
    }

    /**
     * @brief Searches backward for text in the viewer.
     * @param text Target search string.
     * @param matchCase Case sensitivity toggle.
     * @param wrapped Set to true if search wrapped around document boundary.
     * @return True if match found.
     */
    virtual bool findPrev(const QString &text, bool matchCase, bool *wrapped = nullptr) {
        Q_UNUSED(text); Q_UNUSED(matchCase); if (wrapped) *wrapped = false; return false;
    }

    /**
     * @brief Increases display zoom level.
     */
    virtual void zoomIn() {}

    /**
     * @brief Decreases display zoom level.
     */
    virtual void zoomOut() {}

    /**
     * @brief Resets zoom level to default 100% or fit mode.
     */
    virtual void resetZoom() {}

    /**
     * @brief Rotates view 90 degrees clockwise.
     */
    virtual void rotateClockwise() {}

    /**
     * @brief Rotates view 90 degrees counter-clockwise.
     */
    virtual void rotateCounterClockwise() {}
};
