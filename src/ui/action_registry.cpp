#include "action_registry.h"
#include "base_skin.h"
#include "image_viewer.h"
#include "core/interfaces/i_pdf_viewer.h"
#include "core/interfaces/i_media_viewer.h"
#include "office_viewer.h"
#include "text_viewer.h"
#include "core/interfaces/i_archive_viewer.h"
#include "build_config.h"

QList<UIActionItem> ActionRegistry::getActionsForCategory(FileCategory category, BaseSkin *skin)
{
    QList<UIActionItem> actions;
    if (!skin) return actions;

    // --- 1. Universal Navigation Actions ---
    if (category != FileCategory::Folder) {
        UIActionItem prevAct;
        prevAct.id = "prev_file";
        prevAct.label = QObject::tr("◀ Previous");
        prevAct.iconName = "go-previous";
        prevAct.shortcut = QKeySequence(Qt::Key_Left);
        prevAct.category = "nav";
        prevAct.trigger = [skin]() { skin->prevFile(); };
        actions.append(prevAct);

        UIActionItem nextAct;
        nextAct.id = "next_file";
        nextAct.label = QObject::tr("Next ▶");
        nextAct.iconName = "go-next";
        nextAct.shortcut = QKeySequence(Qt::Key_Right);
        nextAct.category = "nav";
        nextAct.trigger = [skin]() { skin->nextFile(); };
        actions.append(nextAct);
    }

    // --- 2. Category-Specific Actions ---
    switch (category) {
    case FileCategory::Pdf: {
        // Page Navigation
        UIActionItem prevPage;
        prevPage.id = "pdf_prev_page";
        prevPage.label = QObject::tr("▲ Prev Page");
        prevPage.iconName = "go-up";
        prevPage.shortcut = QKeySequence(Qt::Key_PageUp);
        prevPage.category = "view";
        prevPage.trigger = [skin]() { skin->pdfViewer()->prevPage(); };
        actions.append(prevPage);

        UIActionItem nextPage;
        nextPage.id = "pdf_next_page";
        nextPage.label = QObject::tr("▼ Next Page");
        nextPage.iconName = "go-down";
        nextPage.shortcut = QKeySequence(Qt::Key_PageDown);
        nextPage.category = "view";
        nextPage.trigger = [skin]() { skin->pdfViewer()->nextPage(); };
        actions.append(nextPage);

        // Zoom & Fit
        UIActionItem zoomTool;
        zoomTool.id = "zoom_tool";
        zoomTool.label = QObject::tr("🔍 Zoom");
        zoomTool.iconName = "zoom-fit-best";
        zoomTool.category = "view";
        zoomTool.isCheckable = true;
        zoomTool.isChecked = [skin]() { return skin->isZoomToolActive(); };
        zoomTool.trigger = [skin]() {
            skin->toggleZoomTool(!skin->isZoomToolActive());
        };
        actions.append(zoomTool);

        UIActionItem zoomReset;
        zoomReset.id = "zoom_reset";
        zoomReset.label = QObject::tr("↺ 100%");
        zoomReset.iconName = "zoom-original";
        zoomReset.category = "view";
        zoomReset.trigger = [skin]() { skin->resetZoomCurrent(); };
        actions.append(zoomReset);

        UIActionItem fitWidth;
        fitWidth.id = "fit_width";
        fitWidth.label = QObject::tr("↔ Fit Width");
        fitWidth.iconName = "zoom-fit-width";
        fitWidth.category = "view";
        fitWidth.trigger = [skin]() { skin->fitCurrent(); };
        actions.append(fitWidth);

        UIActionItem rotLeft;
        rotLeft.id = "rotate_left";
        rotLeft.label = QObject::tr("↺ Rotate Left");
        rotLeft.iconName = "object-rotate-left";
        rotLeft.shortcut = QKeySequence("L");
        rotLeft.category = "transform";
        rotLeft.trigger = [skin]() { skin->rotateCurrentLeft(); };
        actions.append(rotLeft);

        UIActionItem rotRight;
        rotRight.id = "rotate_right";
        rotRight.label = QObject::tr("↻ Rotate Right");
        rotRight.iconName = "object-rotate-right";
        rotRight.shortcut = QKeySequence("R");
        rotRight.category = "transform";
        rotRight.trigger = [skin]() { skin->rotateCurrentRight(); };
        actions.append(rotRight);

        UIActionItem searchDoc;
        searchDoc.id = "search_doc";
        searchDoc.label = QObject::tr("🔍 Find");
        searchDoc.iconName = "edit-find";
        searchDoc.shortcut = QKeySequence("Ctrl+F");
        searchDoc.category = "tools";
        searchDoc.trigger = [skin]() { skin->searchInDocument(); };
        actions.append(searchDoc);

        if (BuildConfig::isQpdfAvailable()) {
            UIActionItem organizePages;
            organizePages.id = "organize_pdf";
            organizePages.label = QObject::tr("📑 Pages");
            organizePages.iconName = "document-edit";
            organizePages.category = "tools";
            organizePages.trigger = [skin]() { skin->openPdfPageOrganizer(); };
            actions.append(organizePages);

            UIActionItem repairPdf;
            repairPdf.id = "repair_pdf";
            repairPdf.label = QObject::tr("🔧 Repair");
            repairPdf.iconName = "tools-check";
            repairPdf.category = "tools";
            repairPdf.trigger = [skin]() { skin->openPdfRepairDialog(); };
            actions.append(repairPdf);
        }

        // PDF Annotation & Drawing Tools
        UIActionItem undoPdf;
        undoPdf.id = "undo";
        undoPdf.label = QObject::tr("↩ Undo");
        undoPdf.iconName = "edit-undo";
        undoPdf.shortcut = QKeySequence::Undo;
        undoPdf.category = "edit";
        undoPdf.isEnabled = [skin]() { return skin->pdfViewer() && skin->pdfViewer()->canUndo(); };
        undoPdf.trigger = [skin]() { skin->pdfViewer()->undo(); };
        actions.append(undoPdf);

        UIActionItem redoPdf;
        redoPdf.id = "redo";
        redoPdf.label = QObject::tr("↪ Redo");
        redoPdf.iconName = "edit-redo";
        redoPdf.shortcut = QKeySequence::Redo;
        redoPdf.category = "edit";
        redoPdf.isEnabled = [skin]() { return skin->pdfViewer() && skin->pdfViewer()->canRedo(); };
        redoPdf.trigger = [skin]() { skin->pdfViewer()->redo(); };
        actions.append(redoPdf);

        UIActionItem cutPdf;
        cutPdf.id = "pdf_cut_text";
        cutPdf.label = QObject::tr("✂️ Cut Text");
        cutPdf.iconName = "edit-cut";
        cutPdf.category = "edit";
        cutPdf.trigger = [skin]() { skin->pdfViewer()->cutSelectedText(); };
        actions.append(cutPdf);

        UIActionItem pastePdf;
        pastePdf.id = "pdf_paste_text";
        pastePdf.label = QObject::tr("📋 Paste Text");
        pastePdf.iconName = "edit-paste";
        pastePdf.category = "edit";
        pastePdf.trigger = [skin]() { skin->pdfViewer()->pasteText(); };
        actions.append(pastePdf);

        UIActionItem typewriter;
        typewriter.id = "pdf_typewriter";
        typewriter.label = QObject::tr("✍️ Typewriter");
        typewriter.iconName = "document-edit";
        typewriter.category = "edit";
        typewriter.trigger = [skin]() { skin->pdfViewer()->openTypewriterDialog(); };
        actions.append(typewriter);

        UIActionItem signature;
        signature.id = "pdf_signature";
        signature.label = QObject::tr("🔏 Signature");
        signature.iconName = "mail-signed";
        signature.category = "edit";
        signature.trigger = [skin]() { skin->pdfViewer()->openSignatureDialog(); };
        actions.append(signature);

        UIActionItem cryptoSig;
        cryptoSig.id = "pdf_crypto_signature";
        cryptoSig.label = QObject::tr("🔐 Digital Signature");
        cryptoSig.iconName = "security-high";
        cryptoSig.category = "tools";
        cryptoSig.trigger = [skin]() { skin->pdfViewer()->openCryptoSignatureDialog(0); };
        actions.append(cryptoSig);

        UIActionItem insertImage;
        insertImage.id = "pdf_insert_image";
        insertImage.label = QObject::tr("🖼️ Insert Image");
        insertImage.iconName = "insert-image";
        insertImage.category = "edit";
        insertImage.trigger = [skin]() { skin->pdfViewer()->openInsertImageDialog(); };
        actions.append(insertImage);

        UIActionItem insertPage;
        insertPage.id = "pdf_insert_page";
        insertPage.label = QObject::tr("➕ Insert Blank Page");
        insertPage.iconName = "document-new";
        insertPage.category = "edit";
        insertPage.trigger = [skin]() { skin->pdfViewer()->insertBlankPage(); };
        actions.append(insertPage);

        UIActionItem watermark;
        watermark.id = "pdf_watermark";
        watermark.label = QObject::tr("💧 Watermark");
        watermark.iconName = "format-text-underline";
        watermark.category = "edit";
        watermark.trigger = [skin]() { skin->pdfViewer()->openWatermarkDialog(); };
        actions.append(watermark);

        UIActionItem shape;
        shape.id = "pdf_shape";
        shape.label = QObject::tr("📐 Shape");
        shape.iconName = "draw-rectangle";
        shape.category = "edit";
        shape.trigger = [skin]() { skin->pdfViewer()->openShapeDialog(); };
        actions.append(shape);

        UIActionItem snapshot;
        snapshot.id = "pdf_snapshot";
        snapshot.label = QObject::tr("📸 Snapshot");
        snapshot.iconName = "camera-photo";
        snapshot.category = "tools";
        snapshot.trigger = [skin]() { skin->pdfViewer()->startSnapshot(); };
        actions.append(snapshot);

        UIActionItem redact;
        redact.id = "pdf_redact";
        redact.label = QObject::tr("⬛ Redact");
        redact.iconName = "edit-clear";
        redact.category = "edit";
        redact.trigger = [skin]() { skin->pdfViewer()->startRedaction(); };
        actions.append(redact);
        break;
    }

    case FileCategory::Image: {
        UIActionItem zoomTool;
        zoomTool.id = "zoom_tool";
        zoomTool.label = QObject::tr("🔍 Zoom");
        zoomTool.iconName = "zoom-fit-best";
        zoomTool.category = "view";
        zoomTool.isCheckable = true;
        zoomTool.isChecked = [skin]() { return skin->isZoomToolActive(); };
        zoomTool.trigger = [skin]() {
            skin->toggleZoomTool(!skin->isZoomToolActive());
        };
        actions.append(zoomTool);

        UIActionItem zoomIn;
        zoomIn.id = "zoom_in";
        zoomIn.label = QObject::tr("🔍 +");
        zoomIn.iconName = "zoom-in";
        zoomIn.category = "view";
        zoomIn.trigger = [skin]() { skin->zoomInCurrent(); };
        actions.append(zoomIn);

        UIActionItem zoomOut;
        zoomOut.id = "zoom_out";
        zoomOut.label = QObject::tr("🔍 -");
        zoomOut.iconName = "zoom-out";
        zoomOut.category = "view";
        zoomOut.trigger = [skin]() { skin->zoomOutCurrent(); };
        actions.append(zoomOut);

        UIActionItem zoomReset;
        zoomReset.id = "zoom_reset";
        zoomReset.label = QObject::tr("↺ 100%");
        zoomReset.iconName = "zoom-original";
        zoomReset.category = "view";
        zoomReset.trigger = [skin]() { skin->resetZoomCurrent(); };
        actions.append(zoomReset);

        UIActionItem fitWidth;
        fitWidth.id = "fit_width";
        fitWidth.label = QObject::tr("↔ Fit to Window");
        fitWidth.iconName = "zoom-fit-width";
        fitWidth.category = "view";
        fitWidth.trigger = [skin]() { skin->fitCurrent(); };
        actions.append(fitWidth);

        UIActionItem rotLeft;
        rotLeft.id = "rotate_left";
        rotLeft.label = QObject::tr("↺ Rotate Left");
        rotLeft.iconName = "object-rotate-left";
        rotLeft.shortcut = QKeySequence("L");
        rotLeft.category = "transform";
        rotLeft.trigger = [skin]() { skin->rotateCurrentLeft(); };
        actions.append(rotLeft);

        UIActionItem rotRight;
        rotRight.id = "rotate_right";
        rotRight.label = QObject::tr("↻ Rotate Right");
        rotRight.iconName = "object-rotate-right";
        rotRight.shortcut = QKeySequence("R");
        rotRight.category = "transform";
        rotRight.trigger = [skin]() { skin->rotateCurrentRight(); };
        actions.append(rotRight);

        UIActionItem resizeAct;
        resizeAct.id = "image_resize";
        resizeAct.label = QObject::tr("📐 Resize Image");
        resizeAct.iconName = "transform-scale";
        resizeAct.shortcut = QKeySequence("Ctrl+R");
        resizeAct.category = "edit";
        resizeAct.trigger = [skin]() { skin->openResizeImageDialog(); };
        actions.append(resizeAct);

        UIActionItem adjustAct;
        adjustAct.id = "image_adjust";
        adjustAct.label = QObject::tr("🎨 Photo Studio & Filters");
        adjustAct.iconName = "adjustlevels";
        adjustAct.shortcut = QKeySequence("Ctrl+B");
        adjustAct.category = "edit";
        adjustAct.trigger = [skin]() { skin->openAdjustImageDialog(); };
        actions.append(adjustAct);

        UIActionItem inspectorAct;
        inspectorAct.id = "image_inspector";
        inspectorAct.label = QObject::tr("ℹ️ Properties & EXIF");
        inspectorAct.iconName = "document-properties";
        inspectorAct.shortcut = QKeySequence("Alt+Return");
        inspectorAct.category = "tools";
        inspectorAct.trigger = [skin]() { skin->showFileInfo(); };
        actions.append(inspectorAct);
        break;
    }

    case FileCategory::Text: {
        UIActionItem searchDoc;
        searchDoc.id = "search_doc";
        searchDoc.label = QObject::tr("🔍 Find");
        searchDoc.iconName = "edit-find";
        searchDoc.shortcut = QKeySequence("Ctrl+F");
        searchDoc.category = "tools";
        searchDoc.trigger = [skin]() { skin->searchInDocument(); };
        actions.append(searchDoc);

        UIActionItem findReplace;
        findReplace.id = "find_replace";
        findReplace.label = QObject::tr("⇄ Replace");
        findReplace.iconName = "edit-find-replace";
        findReplace.shortcut = QKeySequence("Ctrl+H");
        findReplace.category = "tools";
        findReplace.trigger = [skin]() { skin->showFindReplace(); };
        actions.append(findReplace);

        UIActionItem editText;
        editText.id = "edit_text";
        editText.label = QObject::tr("✏️ Edit Text");
        editText.iconName = "document-edit";
        editText.shortcut = QKeySequence("F4");
        editText.category = "tools";
        editText.isCheckable = true;
        editText.isChecked = [skin]() {
            return skin->textViewer() && skin->textViewer()->isEditable();
        };
        editText.trigger = [skin]() {
            if (skin->textViewer()) {
                skin->textViewer()->setEditable(!skin->textViewer()->isEditable());
            }
        };
        actions.append(editText);

        UIActionItem lineNums;
        lineNums.id = "line_numbers";
        lineNums.label = QObject::tr("# Lines");
        lineNums.iconName = "format-list-ordered";
        lineNums.shortcut = QKeySequence("F8");
        lineNums.category = "view";
        lineNums.isCheckable = true;
        lineNums.isChecked = [skin]() -> bool {
            return skin->textViewer() && skin->textViewer()->isLineNumbersVisible();
        };
        lineNums.trigger = [skin]() {
            if (skin->textViewer()) {
                skin->textViewer()->setLineNumbersVisible(!skin->textViewer()->isLineNumbersVisible());
            }
        };
        actions.append(lineNums);

        UIActionItem syntaxHigh;
        syntaxHigh.id = "syntax_highlight";
        syntaxHigh.label = QObject::tr("{ } Syntax");
        syntaxHigh.iconName = "code-class";
        syntaxHigh.shortcut = QKeySequence("F9");
        syntaxHigh.category = "view";
        syntaxHigh.isCheckable = true;
        syntaxHigh.isChecked = [skin]() -> bool {
            return skin->textViewer() && skin->textViewer()->isSyntaxHighlightingEnabled();
        };
        syntaxHigh.trigger = [skin]() {
            if (skin->textViewer()) {
                skin->textViewer()->setSyntaxHighlightingEnabled(!skin->textViewer()->isSyntaxHighlightingEnabled());
            }
        };
        actions.append(syntaxHigh);

        UIActionItem wordWrap;
        wordWrap.id = "word_wrap";
        wordWrap.label = QObject::tr("↩ Wrap");
        wordWrap.iconName = "format-justify-fill";
        wordWrap.shortcut = QKeySequence("Alt+Z");
        wordWrap.category = "view";
        wordWrap.isCheckable = true;
        wordWrap.isChecked = [skin]() -> bool {
            return skin->textViewer() && skin->textViewer()->isWordWrapEnabled();
        };
        wordWrap.trigger = [skin]() {
            if (skin->textViewer()) {
                skin->textViewer()->setWordWrapEnabled(!skin->textViewer()->isWordWrapEnabled());
            }
        };
        actions.append(wordWrap);

        UIActionItem gotoLine;
        gotoLine.id = "goto_line";
        gotoLine.label = QObject::tr("↷ Go to Line");
        gotoLine.iconName = "go-jump";
        gotoLine.shortcut = QKeySequence("Ctrl+G");
        gotoLine.category = "tools";
        gotoLine.trigger = [skin]() { skin->gotoLineInDocument(); };
        actions.append(gotoLine);
        break;
    }

    case FileCategory::Media: {
        UIActionItem playPause;
        playPause.id = "media_play_pause";
        playPause.label = QObject::tr("⏯ Play / Pause");
        playPause.iconName = "media-playback-start";
        playPause.shortcut = QKeySequence(Qt::Key_Space);
        playPause.category = "view";
        playPause.trigger = [skin]() { skin->mediaViewer()->togglePlayPause(); };
        actions.append(playPause);
        break;
    }

    case FileCategory::Office: {
        if (skin->officeViewer()->isPdfMode()) {
            UIActionItem prevPage;
            prevPage.id = "pdf_prev_page";
            prevPage.label = QObject::tr("▲ Prev Page");
            prevPage.iconName = "go-up";
            prevPage.shortcut = QKeySequence(Qt::Key_PageUp);
            prevPage.category = "view";
            prevPage.trigger = [skin]() { skin->officeViewer()->pdfViewer()->prevPage(); };
            actions.append(prevPage);

            UIActionItem nextPage;
            nextPage.id = "pdf_next_page";
            nextPage.label = QObject::tr("▼ Next Page");
            nextPage.iconName = "go-down";
            nextPage.shortcut = QKeySequence(Qt::Key_PageDown);
            nextPage.category = "view";
            nextPage.trigger = [skin]() { skin->officeViewer()->pdfViewer()->nextPage(); };
            actions.append(nextPage);

            UIActionItem zoomIn;
            zoomIn.id = "zoom_in";
            zoomIn.label = QObject::tr("🔍 +");
            zoomIn.iconName = "zoom-in";
            zoomIn.category = "view";
            zoomIn.trigger = [skin]() { skin->officeViewer()->pdfViewer()->zoomIn(); };
            actions.append(zoomIn);

            UIActionItem zoomOut;
            zoomOut.id = "zoom_out";
            zoomOut.label = QObject::tr("🔍 -");
            zoomOut.iconName = "zoom-out";
            zoomOut.category = "view";
            zoomOut.trigger = [skin]() { skin->officeViewer()->pdfViewer()->zoomOut(); };
            actions.append(zoomOut);

            UIActionItem rotLeft;
            rotLeft.id = "rotate_left";
            rotLeft.label = QObject::tr("↺ Rotate Left");
            rotLeft.iconName = "object-rotate-left";
            rotLeft.shortcut = QKeySequence("L");
            rotLeft.category = "transform";
            rotLeft.trigger = [skin]() { skin->rotateCurrentLeft(); };
            actions.append(rotLeft);

            UIActionItem rotRight;
            rotRight.id = "rotate_right";
            rotRight.label = QObject::tr("↻ Rotate Right");
            rotRight.iconName = "object-rotate-right";
            rotRight.shortcut = QKeySequence("R");
            rotRight.category = "transform";
            rotRight.trigger = [skin]() { skin->rotateCurrentRight(); };
            actions.append(rotRight);
        }
        break;
    }

    case FileCategory::Archive: {
        UIActionItem addFilesAct;
        addFilesAct.id = "archive_add_files";
        addFilesAct.label = QObject::tr("➕ Add Files...");
        addFilesAct.iconName = "list-add";
        addFilesAct.category = "edit";
        addFilesAct.trigger = [skin]() { skin->addFilesToCurrentArchive(); };
        actions.append(addFilesAct);

        UIActionItem addFolderAct;
        addFolderAct.id = "archive_add_folder";
        addFolderAct.label = QObject::tr("📁 Add Folder...");
        addFolderAct.iconName = "folder-new";
        addFolderAct.category = "edit";
        addFolderAct.trigger = [skin]() { skin->addFolderToCurrentArchive(); };
        actions.append(addFolderAct);

        UIActionItem removeSelAct;
        removeSelAct.id = "archive_remove_selected";
        removeSelAct.label = QObject::tr("🗑️ Remove Selected");
        removeSelAct.iconName = "edit-delete";
        removeSelAct.category = "edit";
        removeSelAct.trigger = [skin]() { skin->removeSelectedFromCurrentArchive(); };
        actions.append(removeSelAct);

        UIActionItem extractAct;
        extractAct.id = "archive_extract_all";
        extractAct.label = QObject::tr("📦 Extract All");
        extractAct.iconName = "archive-extract";
        extractAct.category = "tools";
        extractAct.trigger = [skin]() { skin->archiveViewer()->extractAll(); };
        actions.append(extractAct);

        UIActionItem testAct;
        testAct.id = "archive_test";
        testAct.label = QObject::tr("🛡️ Test");
        testAct.iconName = "system-run";
        testAct.category = "tools";
        testAct.trigger = [skin]() { skin->archiveViewer()->testArchiveIntegrity(); };
        actions.append(testAct);
        break;
    }

    default:
        break;
    }

    // --- 3. Export / File Actions ---
    if (category == FileCategory::Image || category == FileCategory::Pdf || category == FileCategory::Text) {
        UIActionItem saveAct;
        saveAct.id = "save_file";
        saveAct.label = QObject::tr("💾 Save");
        saveAct.iconName = "document-save";
        saveAct.shortcut = QKeySequence::Save;
        saveAct.category = "file";
        saveAct.trigger = [skin]() { skin->saveCurrentFile(); };
        actions.append(saveAct);
    }

    if (category == FileCategory::Image || category == FileCategory::Pdf ||
        category == FileCategory::Text || category == FileCategory::Office) {
        UIActionItem exportAct;
        exportAct.id = "export_as";
        exportAct.label = QObject::tr("📤 Export");
        exportAct.iconName = "document-export";
        exportAct.shortcut = QKeySequence("Ctrl+E");
        exportAct.category = "file";
        exportAct.trigger = [skin]() { skin->exportCurrentFileAs(); };
        actions.append(exportAct);
    }

    UIActionItem delAct;
    delAct.id = "delete_file";
    delAct.label = QObject::tr("🗑️ Trash");
    delAct.iconName = "user-trash";
    delAct.shortcut = QKeySequence(Qt::Key_Delete);
    delAct.category = "edit";
    delAct.trigger = [skin]() { skin->deleteCurrentFile(); };
    actions.append(delAct);

    // --- 4. App / Window Actions ---
    UIActionItem openFolder;
    openFolder.id = "open_folder";
    openFolder.label = QObject::tr("📁 Open Folder...");
    openFolder.iconName = "folder-open";
    openFolder.shortcut = QKeySequence("Ctrl+Shift+O");
    openFolder.category = "app";
    openFolder.trigger = [skin]() { skin->openFolderDialog(); };
    actions.append(openFolder);

    UIActionItem fullscreen;
    fullscreen.id = "fullscreen";
    fullscreen.label = QObject::tr("⛶ Fullscreen");
    fullscreen.iconName = "view-fullscreen";
    fullscreen.shortcut = QKeySequence("F11");
    fullscreen.category = "app";
    fullscreen.trigger = [skin]() { skin->toggleFullscreen(); };
    actions.append(fullscreen);

    UIActionItem settings;
    settings.id = "settings";
    settings.label = QObject::tr("⚙️ Settings");
    settings.iconName = "preferences-system";
    settings.category = "app";
    settings.trigger = [skin]() { skin->openAppearanceSettings(); };
    actions.append(settings);

    return actions;
}
