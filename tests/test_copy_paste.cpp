#include <QApplication>
#include <QTest>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QClipboard>
#include <QMimeData>
#include <QJsonObject>
#include <QJsonDocument>
#include <poppler-qt6.h>
#include <poppler-annotation.h>
#include <iostream>

#define private public
#define protected public
#include "plugins/pdf/pdf_page_widget.h"
#include "plugins/pdf/pdf_transform_box.h"
#include "viewers/pdf/pdf_document_ops.h"
#include "core/system/font_resolution_helper.h"
#include "ui/dialogs/font_resolution_dialog.h"

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    auto doc = Poppler::Document::load("/home/davide/Progetti/BubezView/test/verticale.pdf");
    if (!doc) {
        std::cerr << "Doc load failed\n";
        return 1;
    }

    PdfPageWidget widget;
    widget.setFilePath("/home/davide/Progetti/BubezView/test/verticale.pdf");
    widget.setEditMode(true);
    // Page 2 (index 1), zoom 1.2 (120%)
    widget.setDocument(doc.get(), 1.2, false, 0, 1600);
    widget.setActivePage(1);

    // Connetti addStampAnnotationRequested per cancellare/tagliare il testo originale
    QObject::connect(&widget, &PdfPageWidget::addStampAnnotationRequested,
                     [&doc](const QImage &img, const QRectF &normRect) {
        auto page = doc->page(1);
        if (!page) return;
        auto *bgStamp = new Poppler::StampAnnotation();
        bgStamp->setStampCustomImage(img);
        bgStamp->setBoundary(normRect);
        page->addAnnotation(bgStamp);
        delete bgStamp;
    });

    // Connetti addTextAnnotationRequested per applicare direttamente la TextAnnotation vettoriale sulla pagina Poppler
    QObject::connect(&widget, &PdfPageWidget::addTextAnnotationRequested,
                     [&doc](const QString &text, const QFont &font, const QColor &color,
                            const QRectF &normalizedRect, bool /*whiteout*/, const QImage &/*bgImage*/,
                            const QRectF &/*bgNormalizedRect*/) {
        auto page = doc->page(1);
        if (!page) return;

        auto *ann = new Poppler::TextAnnotation(Poppler::TextAnnotation::InPlace);
        ann->setInplaceIntent(Poppler::TextAnnotation::TypeWriter);
        ann->setInplaceAlign(Poppler::TextAnnotation::InplaceAlignLeft);
        ann->setContents(text);

        QFont annotFont = font;
        if (annotFont.bold() && annotFont.italic()) {
            annotFont.setStyleName(QStringLiteral("Bold Italic"));
        } else if (annotFont.bold()) {
            annotFont.setStyleName(QStringLiteral("Bold"));
        } else if (annotFont.italic()) {
            annotFont.setStyleName(QStringLiteral("Italic"));
        }

        ann->setTextFont(annotFont);
        ann->setTextColor(color);
        ann->setFlags(Poppler::Annotation::FixedRotation);
        ann->setBoundary(normalizedRect);

        Poppler::Annotation::Style annotStyle = ann->style();
        annotStyle.setWidth(0.0);
        annotStyle.setColor(QColor(0, 0, 0, 0));
        ann->setStyle(annotStyle);

        page->addAnnotation(ann);
        delete ann;
    });

    // Seleziona il testo "Mille volti da guardare..."
    for (size_t i = 0; i < widget.m_textBoxes.size(); ++i) {
        QString t = widget.m_textBoxes[i]->text();
        if (t.contains("Mille") || (!widget.m_selectedIndices.empty() && widget.m_selectedIndices.size() < 30)) {
            widget.m_selectedIndices.push_back((int)i);
            if (t.contains("pasticcerie")) break;
        }
    }

    // MODIFICA TESTO SUL POSTO (In-Place Edit / Replace Text)
    widget.startInPlaceTextEdit();

    // Modifica il contenuto del testo
    if (widget.m_transformBox) {
        widget.m_transformBox->updateTextContent("Trieste, una vista meravigliosa sul mondo intero!");
    }

    if (widget.m_transformBox && widget.m_transformBox->m_btnFontStatus) {
        std::cout << "Font status button text: "
                  << widget.m_transformBox->m_btnFontStatus->text().toStdString() << "\n";
        std::cout << "Font status tooltip: "
                  << widget.m_transformBox->m_btnFontStatus->toolTip().toStdString() << "\n";
    }

    // 2. TEST FONT RESOLUTION DIALOG UI:
    FontResolutionDialog dlg("TradeGothic-Bold", "", widget.m_transformBox->textFont(), nullptr);
    dlg.resize(580, 480);
    QImage dlgImg(dlg.size(), QImage::Format_ARGB32_Premultiplied);
    dlgImg.fill(Qt::transparent);
    dlg.render(&dlgImg);
    dlgImg.save("/home/davide/Progetti/BubezView/test/font_resolution_dialog.png");
    std::cout << "Saved font resolution dialog screenshot to test/font_resolution_dialog.png\n";

    // 3. APPLICA IL TESTO DEFINITIVAMENTE (scrittura vettoriale su PDF)
    widget.onTransformBoxApplied();

    // Renderizza la pagina Poppler reale a 120% (1.2 * 72 = 86.4 DPI)
    auto page = doc->page(1);
    double dpi = 1.2 * 72.0;
    QImage finalPageImg = page->renderToImage(dpi, dpi);
    finalPageImg.save("/home/davide/Progetti/BubezView/test/screenshot_applicato.png");
    std::cout << "Saved screenshot to test/screenshot_applicato.png\n";

    // =========================================================================
    // 4. TEST SELECTION, COPY, CUT, PASTE WITH NATIVE MUPDF FORMAT INSPECTION
    // =========================================================================
    std::cout << "\n--- Testing Native Font Inspection, Copy, Cut & Paste ---\n";
    const QString millimetricaPath = QStringLiteral("/home/davide/Progetti/BubezView/test/carta_millimetrica.pdf");
    auto docMilli = Poppler::Document::load(millimetricaPath);
    if (!docMilli) {
        std::cerr << "ERRORE: Impossibile caricare " << millimetricaPath.toStdString() << "\n";
        return 1;
    }

    PdfPageWidget widgetMilli;
    widgetMilli.setFilePath(millimetricaPath);
    widgetMilli.setEditMode(true);
    widgetMilli.resize(1000, 1400);
    widgetMilli.show();
    widgetMilli.setDocument(docMilli.get(), 1.0, false, 0, 1200);
    widgetMilli.setActivePage(0);
    widgetMilli.refreshActivePageData();

    // Setup signal monitors
    QString emittedSelText;
    QFont emittedSelFont;
    QColor emittedSelColor;
    bool selSignalFired = false;
    QObject::connect(&widgetMilli, &PdfPageWidget::textSelectedForContextualEdit,
                     [&](const QString &t, const QFont &f, const QColor &c, bool) {
        emittedSelText = t;
        emittedSelFont = f;
        emittedSelColor = c;
        selSignalFired = true;
    });

    QString emittedPasteText;
    QFont emittedPasteFont;
    QColor emittedPasteColor;
    bool pasteSignalFired = false;
    QObject::connect(&widgetMilli, &PdfPageWidget::inPlaceTextEditStarted,
                     [&](const QString &t, const QFont &f, const QColor &c, PdfTextBgMode, const QColor &) {
        emittedPasteText = t;
        emittedPasteFont = f;
        emittedPasteColor = c;
        pasteSignalFired = true;
    });

    bool eraseSignalFired = false;
    QRectF erasedNormRect;
    QObject::connect(&widgetMilli, &PdfPageWidget::eraseOriginalTextRequested,
                     [&](int, const QRectF &, const QRectF &norm) {
        eraseSignalFired = true;
        erasedNormRect = norm;
    });

    // 4.1 Test Selection on "Display Titolo" (32pt)
    int idxDisplayTitolo = -1;
    for (size_t i = 0; i < widgetMilli.m_textBoxes.size(); ++i) {
        if (widgetMilli.m_textBoxes[i]->text().contains("Display")) {
            idxDisplayTitolo = static_cast<int>(i);
            break;
        }
    }
    Q_ASSERT(idxDisplayTitolo >= 0);

    widgetMilli.m_selectedIndices = {idxDisplayTitolo};
    widgetMilli.captureSelectedTextFormat();
    std::cout << "[Test 4.1] Selected word: '" << widgetMilli.selectedText().toStdString()
              << "' Font: " << widgetMilli.m_lastCopiedTextFormat.font.family().toStdString()
              << " " << widgetMilli.m_lastCopiedTextFormat.font.pointSizeF() << "pt"
              << " Color: " << widgetMilli.m_lastCopiedTextFormat.color.name().toStdString() << "\n";

    Q_ASSERT(widgetMilli.m_lastCopiedTextFormat.valid);
    Q_ASSERT(qRound(widgetMilli.m_lastCopiedTextFormat.font.pointSizeF()) == 32);

    // 4.2 Test Copy rich MIME data to clipboard
    widgetMilli.copySelectionToClipboard();
    const QMimeData *clipMime = QApplication::clipboard()->mimeData();
    Q_ASSERT(clipMime != nullptr);
    Q_ASSERT(clipMime->hasFormat("application/x-bubez-text-format"));
    Q_ASSERT(clipMime->hasText());

    QJsonDocument clipJson = QJsonDocument::fromJson(clipMime->data("application/x-bubez-text-format"));
    Q_ASSERT(!clipJson.isNull() && clipJson.isObject());
    QJsonObject clipObj = clipJson.object();
    std::cout << "[Test 4.2] Clipboard JSON: family=" << clipObj["family"].toString().toStdString()
              << " pt=" << clipObj["pointSize"].toDouble()
              << " color=" << clipObj["color"].toString().toStdString() << "\n";
    Q_ASSERT(qRound(clipObj["pointSize"].toDouble()) == 32);

    // 4.3 Test Paste from clipboard
    pasteSignalFired = false;
    widgetMilli.m_lastCopiedTextFormat = PdfPageWidget::CopiedTextFormat(); // reset cache to prove MIME reading
    widgetMilli.pasteFromClipboard(QPoint(200, 300));

    Q_ASSERT(pasteSignalFired);
    std::cout << "[Test 4.3] Paste Signal Fired: text='" << emittedPasteText.toStdString()
              << "' font=" << emittedPasteFont.family().toStdString()
              << " " << emittedPasteFont.pointSizeF() << "pt"
              << " color=" << emittedPasteColor.name().toStdString() << "\n";
    Q_ASSERT(qRound(emittedPasteFont.pointSizeF()) == 32);
    Q_ASSERT(widgetMilli.m_transformBox != nullptr);
    Q_ASSERT(widgetMilli.m_transformBox->isVisible());

    // 4.4 Test Cut text on "Grande" (24pt)
    int idx24 = -1;
    for (size_t i = 0; i < widgetMilli.m_textBoxes.size(); ++i) {
        if (widgetMilli.m_textBoxes[i]->text().contains("Grande")) {
            idx24 = static_cast<int>(i);
            break;
        }
    }
    if (idx24 < 0) {
        std::cerr << "ERRORE: 'Grande' non trovato nei text box!\n";
        return 1;
    }

    eraseSignalFired = false;
    widgetMilli.m_selectedIndices = {idx24};
    widgetMilli.cutSelection();

    if (!eraseSignalFired) {
        std::cerr << "ERRORE: eraseOriginalTextRequested non emesso durante cutSelection!\n";
        return 1;
    }
    std::cout << "[Test 4.4] Cut operation completed: eraseSignalFired=" << eraseSignalFired
              << " erasedNormRect=" << erasedNormRect.x() << "," << erasedNormRect.y()
              << " " << erasedNormRect.width() << "x" << erasedNormRect.height() << "\n";

    clipMime = QApplication::clipboard()->mimeData();
    if (!clipMime || !clipMime->hasFormat("application/x-bubez-text-format")) {
        std::cerr << "ERRORE: Clipboard priva di application/x-bubez-text-format dopo Cut!\n";
        return 1;
    }
    clipJson = QJsonDocument::fromJson(clipMime->data("application/x-bubez-text-format"));
    clipObj = clipJson.object();
    std::cout << "[Test 4.4] Cut Clipboard JSON: pt=" << clipObj["pointSize"].toDouble()
              << " color=" << clipObj["color"].toString().toStdString() << "\n";
    if (qRound(clipObj["pointSize"].toDouble()) != 24) {
        std::cerr << "ERRORE: Atteso pt=24 dopo Cut, ottenuto: " << clipObj["pointSize"].toDouble() << "\n";
        return 1;
    }

    // 4.5 Test Paste after Cut restores exact 24pt
    pasteSignalFired = false;
    widgetMilli.m_lastCopiedTextFormat = PdfPageWidget::CopiedTextFormat();
    widgetMilli.pasteFromClipboard(QPoint(100, 150));
    if (!pasteSignalFired) {
        std::cerr << "ERRORE: inPlaceTextEditStarted non emesso dopo Paste da Cut!\n";
        return 1;
    }
    std::cout << "[Test 4.5] Paste after Cut: text='" << emittedPasteText.toStdString()
              << "' font=" << emittedPasteFont.family().toStdString()
              << " " << emittedPasteFont.pointSizeF() << "pt\n";
    if (qRound(emittedPasteFont.pointSizeF()) != 24) {
        std::cerr << "ERRORE: Atteso font pt=24 dopo Paste, ottenuto: " << emittedPasteFont.pointSizeF() << "\n";
        return 1;
    }

    // 4.6 Test Non-blocking collision on Apply:
    // When box collides with existing text, applying must NOT be blocked by modal dialog
    bool textAnnotationFired = false;
    QRectF appliedTextNormRect;
    QObject::connect(&widgetMilli, &PdfPageWidget::addTextAnnotationRequested,
                     [&](const QString &, const QFont &, const QColor &, const QRectF &norm, bool, const QImage &, const QRectF &) {
        textAnnotationFired = true;
        appliedTextNormRect = norm;
    });

    Q_ASSERT(widgetMilli.m_transformBox != nullptr);
    widgetMilli.m_transformBox->setHasCollision(true);
    widgetMilli.onTransformBoxApplied();

    if (!textAnnotationFired) {
        std::cerr << "ERRORE: onTransformBoxApplied è stato bloccato da collisione invece di procedere!\n";
        return 1;
    }
    std::cout << "[Test 4.6] Non-blocking Collision: Apply succeeded even with collision! normRect="
              << appliedTextNormRect.x() << "," << appliedTextNormRect.y() << " "
              << appliedTextNormRect.width() << "x" << appliedTextNormRect.height() << "\n";

    // 4.7 Test generous text boundary buffer to prevent Poppler from truncating words
    double pageWPt = widgetMilli.m_pageLayouts[0].rect.width() / widgetMilli.m_scaleX;
    double pageHPt = widgetMilli.m_pageLayouts[0].rect.height() / widgetMilli.m_scaleY;
    double appliedWPt = appliedTextNormRect.width() * pageWPt;
    double appliedHPt = appliedTextNormRect.height() * pageHPt;

    QFont appliedFont = widgetMilli.m_transformBox->textFont();
    QFontMetricsF fmPt(appliedFont);
    double naturalWPt = fmPt.horizontalAdvance(widgetMilli.m_transformBox->text());

    std::cout << "[Test 4.7] Word Truncation Guardrail: naturalWidth=" << naturalWPt
              << "pt, appliedWidth=" << appliedWPt << "pt (buffer=" << (appliedWPt - naturalWPt)
              << "pt), appliedHeight=" << appliedHPt << "pt\n";

    if (appliedWPt < naturalWPt + 15.0) {
        std::cerr << "ERRORE: Buffer larghezza testo insufficiente (" << (appliedWPt - naturalWPt) << "pt < 15pt)!\n";
        return 1;
    }

    std::cout << "\n ALL SELECTION, COPY, CUT, PASTE, COLLISION & BUFFER TESTS PASSED SUCCESSFULLY! \n";

    return 0;
}
