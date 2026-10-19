#pragma once

#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>
#include <QStringList>

namespace VectorIcons {

enum class Type {
    Folder,
    FolderDownloads,
    FolderPictures,
    FolderDocuments,
    FolderMusic,
    FolderVideos,
    FolderDesktop,
    Code,
    Zip,
    Image,
    Media,
    Document,
    Pdf,
    Office,
    Text
};

/**
 * @brief Generates high-DPI crisp vector icons with rich colors, subtle depth, and crisp glyphs.
 * @param type Icon category or specific folder type.
 * @param customColor Optional color override for tinting.
 * @return Scalable QIcon containing the rendered 96x96 vector pixmap.
 */
inline QIcon createIcon(Type type, const QColor &customColor = QColor()) {
    QPixmap pix(96, 96);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    switch (type) {
    case Type::Folder:
    case Type::FolderDownloads:
    case Type::FolderPictures:
    case Type::FolderDocuments:
    case Type::FolderMusic:
    case Type::FolderVideos:
    case Type::FolderDesktop: {
        // Vibrant modern folder with 3D depth, tab, peeking sheet, and badge glyph
        QColor tabColor = customColor.isValid() ? customColor : QColor("#38bdf8");
        QColor backColor = customColor.isValid() ? customColor.darker(140) : QColor("#0369a1");
        QColor frontTop = customColor.isValid() ? customColor.lighter(108) : QColor("#38bdf8");
        QColor frontBottom = customColor.isValid() ? customColor : QColor("#0284c7");

        // 1. Top Folder Tab
        QPainterPath tab;
        tab.moveTo(10, 30);
        tab.lineTo(10, 21);
        tab.quadTo(10, 16, 16, 16);
        tab.lineTo(40, 16);
        tab.quadTo(46, 16, 50, 22);
        tab.lineTo(55, 30);
        tab.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(tabColor);
        p.drawPath(tab);

        // 2. Back Body Plate
        p.setBrush(backColor);
        p.drawRoundedRect(QRectF(10, 24, 76, 56), 7, 7);

        // 3. Peeking Document Sheet Inside Folder (gives physical depth)
        p.setBrush(QColor("#f8fafc"));
        p.drawRoundedRect(QRectF(20, 18, 56, 30), 4, 4);
        p.setPen(QPen(QColor("#cbd5e1"), 1.2));
        p.drawLine(26, 25, 48, 25);
        p.drawLine(26, 31, 62, 31);

        // 4. Front Folder Flap (with gradient fill and subtle top-edge highlight)
        p.setPen(Qt::NoPen);
        QLinearGradient flapGrad(0, 34, 0, 84);
        flapGrad.setColorAt(0.0, frontTop);
        flapGrad.setColorAt(1.0, frontBottom);
        p.setBrush(flapGrad);

        QPainterPath front;
        front.moveTo(10, 34);
        front.lineTo(86, 34);
        front.lineTo(81, 79);
        front.quadTo(80, 83, 75, 83);
        front.lineTo(21, 83);
        front.quadTo(16, 83, 15, 79);
        front.lineTo(10, 34);
        p.drawPath(front);

        // Subtle specular highlight line along top flap edge
        p.setPen(QPen(QColor(255, 255, 255, 120), 1.4));
        p.drawLine(11, 35, 85, 35);

        // 5. Specialized Front Glyphs for System Folders
        p.setPen(QPen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        if (type == Type::FolderDownloads) {
            // Arrow pointing down into tray
            p.drawLine(48, 48, 48, 64);
            p.drawLine(42, 58, 48, 64);
            p.drawLine(54, 58, 48, 64);
            p.drawPolyline(QPolygonF() << QPointF(38, 62) << QPointF(38, 69) << QPointF(58, 69) << QPointF(58, 62));
        } else if (type == Type::FolderPictures) {
            // Landscape sun and mountain peaks
            p.setBrush(Qt::white);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QRectF(52, 47, 7, 7));
            p.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(QPolygonF() << QPointF(36, 68) << QPointF(45, 55) << QPointF(52, 64) << QPointF(56, 58) << QPointF(62, 68));
        } else if (type == Type::FolderDocuments) {
            // 3 clean document lines
            p.drawLine(38, 51, 58, 51);
            p.drawLine(38, 58, 58, 58);
            p.drawLine(38, 65, 50, 65);
        } else if (type == Type::FolderMusic) {
            // Beamed musical notes
            p.setBrush(Qt::white);
            p.drawEllipse(QRectF(38, 62, 7, 5));
            p.drawEllipse(QRectF(52, 57, 7, 5));
            p.drawLine(44, 63, 44, 48);
            p.drawLine(58, 58, 58, 43);
            p.setPen(QPen(Qt::white, 2.6, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(44, 48, 58, 43);
        } else if (type == Type::FolderVideos) {
            // Play triangle badge
            p.setBrush(QColor(255, 255, 255, 220));
            p.setPen(Qt::NoPen);
            QPolygonF tri;
            tri << QPointF(44, 50) << QPointF(56, 58) << QPointF(44, 66);
            p.drawPolygon(tri);
        } else if (type == Type::FolderDesktop) {
            // Monitor screen
            p.drawRoundedRect(QRectF(36, 48, 24, 16), 2, 2);
            p.drawLine(48, 64, 48, 69);
            p.drawLine(42, 69, 54, 69);
        }
        break;
    }
    case Type::Zip: {
        // Vibrant modern archive package/folder in warm amber with zipper
        QColor tabColor = customColor.isValid() ? customColor : QColor("#f59e0b");
        QColor bodyColor = customColor.isValid() ? customColor.lighter(108) : QColor("#d97706");
        QColor backColor = QColor("#b45309");

        // Tab
        QPainterPath tab;
        tab.moveTo(10, 30);
        tab.lineTo(10, 21);
        tab.quadTo(10, 16, 16, 16);
        tab.lineTo(40, 16);
        tab.quadTo(46, 16, 50, 22);
        tab.lineTo(55, 30);
        tab.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(tabColor);
        p.drawPath(tab);

        // Back body
        p.setBrush(backColor);
        p.drawRoundedRect(QRectF(10, 24, 76, 56), 7, 7);

        // Front flap
        p.setBrush(bodyColor);
        QPainterPath front;
        front.moveTo(10, 34);
        front.lineTo(86, 34);
        front.lineTo(81, 79);
        front.quadTo(80, 83, 75, 83);
        front.lineTo(21, 83);
        front.quadTo(16, 83, 15, 79);
        front.lineTo(10, 34);
        p.drawPath(front);

        // Vertical zipper band down center
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#334155"));
        p.drawRoundedRect(QRectF(44, 24, 8, 58), 2.0, 2.0);

        // Zipper teeth (alternating gold/silver marks)
        p.setPen(QPen(QColor("#fbbf24"), 1.6));
        for (int y = 26; y <= 76; y += 5) {
            p.drawLine(44, y, 48, y);
            p.drawLine(48, y + 2, 52, y + 2);
        }

        // Zipper pull slider box
        p.setPen(QPen(QColor("#475569"), 1.0));
        p.setBrush(QColor("#cbd5e1"));
        p.drawRoundedRect(QRectF(43, 46, 10, 11), 2.0, 2.0);

        // Zipper pull tab
        QPainterPath pull;
        pull.moveTo(45, 57);
        pull.lineTo(51, 57);
        pull.lineTo(53, 68);
        pull.quadTo(48, 71, 43, 68);
        pull.closeSubpath();
        p.setBrush(QColor("#94a3b8"));
        p.drawPath(pull);
        p.setBrush(QColor("#334155"));
        p.drawEllipse(QRectF(47, 62, 3, 3));
        break;
    }
    case Type::Code: {
        // Dark slate card with folded corner and vibrant syntax brackets
        QRectF card(16, 12, 64, 72);
        QLinearGradient grad(0, 12, 0, 84);
        grad.setColorAt(0.0, QColor("#1e293b"));
        grad.setColorAt(1.0, QColor("#0f172a"));
        p.setPen(QPen(QColor("#334155"), 1.5));
        p.setBrush(grad);
        p.drawRoundedRect(card, 8, 8);

        // Folded top-right corner
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#475569"));
        QPolygonF fold;
        fold << QPointF(64, 12) << QPointF(80, 28) << QPointF(64, 28);
        p.drawPolygon(fold);

        // Syntax code symbol: < / >
        p.setPen(QPen(QColor("#38bdf8"), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        // <
        p.drawLine(34, 43, 27, 49);
        p.drawLine(27, 49, 34, 55);
        // /
        p.drawLine(46, 41, 41, 57);
        // >
        p.drawLine(53, 43, 60, 49);
        p.drawLine(60, 49, 53, 55);

        // Colored syntax lines below
        p.setPen(QPen(QColor("#10b981"), 2.2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(27, 67, 45, 67);
        p.setPen(QPen(QColor("#f59e0b"), 2.2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(49, 67, 65, 67);
        break;
    }
    case Type::Pdf: {
        // Crimson document card with bold white "PDF" pill
        QRectF card(16, 12, 64, 72);
        QLinearGradient grad(0, 12, 0, 84);
        grad.setColorAt(0.0, QColor("#ef4444"));
        grad.setColorAt(1.0, QColor("#b91c1c"));
        p.setPen(QPen(QColor("#dc2626"), 1.5));
        p.setBrush(grad);
        p.drawRoundedRect(card, 8, 8);

        // Folded top-right corner
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#fca5a5"));
        QPolygonF fold;
        fold << QPointF(64, 12) << QPointF(80, 28) << QPointF(64, 28);
        p.drawPolygon(fold);

        // White pill badge
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(26, 42, 44, 20), 5, 5);

        // PDF text
        p.setPen(QColor("#dc2626"));
        QFont f = p.font();
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(26, 42, 44, 20), Qt::AlignCenter, "PDF");
        break;
    }
    case Type::Image: {
        // Violet/indigo card with mountain and sun landscape
        QRectF card(16, 12, 64, 72);
        QLinearGradient grad(0, 12, 0, 84);
        grad.setColorAt(0.0, QColor("#8b5cf6"));
        grad.setColorAt(1.0, QColor("#6366f1"));
        p.setPen(QPen(QColor("#7c3aed"), 1.5));
        p.setBrush(grad);
        p.drawRoundedRect(card, 8, 8);

        // Folded top-right corner
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#c4b5fd"));
        QPolygonF fold;
        fold << QPointF(64, 12) << QPointF(80, 28) << QPointF(64, 28);
        p.drawPolygon(fold);

        // Image Frame
        p.setBrush(QColor("#312e81"));
        p.setPen(QPen(QColor("#a78bfa"), 1.2));
        p.drawRoundedRect(QRectF(26, 36, 44, 34), 4, 4);

        // Sun
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#fbbf24"));
        p.drawEllipse(QRectF(53, 41, 7, 7));

        // Mountain Peaks
        p.setBrush(QColor("#c4b5fd"));
        QPolygonF m1;
        m1 << QPointF(29, 68) << QPointF(40, 52) << QPointF(48, 62) << QPointF(53, 56) << QPointF(63, 68);
        p.drawPolygon(m1);
        break;
    }
    case Type::Media: {
        // Rose/magenta card with sound notes
        QRectF card(16, 12, 64, 72);
        QLinearGradient grad(0, 12, 0, 84);
        grad.setColorAt(0.0, QColor("#ec4899"));
        grad.setColorAt(1.0, QColor("#be185d"));
        p.setPen(QPen(QColor("#db2777"), 1.5));
        p.setBrush(grad);
        p.drawRoundedRect(card, 8, 8);

        // Folded top-right corner
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#fbcfe8"));
        QPolygonF fold;
        fold << QPointF(64, 12) << QPointF(80, 28) << QPointF(64, 28);
        p.drawPolygon(fold);

        // Musical note
        p.setBrush(Qt::white);
        p.setPen(QPen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap));
        p.drawEllipse(QRectF(34, 58, 8, 6));
        p.drawEllipse(QRectF(50, 52, 8, 6));
        p.drawLine(41, 59, 41, 42);
        p.drawLine(57, 53, 57, 36);
        p.setPen(QPen(Qt::white, 2.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(41, 42, 57, 36);
        break;
    }
    case Type::Office: {
        // Emerald/teal card with grid table
        QRectF card(16, 12, 64, 72);
        QLinearGradient grad(0, 12, 0, 84);
        grad.setColorAt(0.0, QColor("#10b981"));
        grad.setColorAt(1.0, QColor("#047857"));
        p.setPen(QPen(QColor("#059669"), 1.5));
        p.setBrush(grad);
        p.drawRoundedRect(card, 8, 8);

        // Folded corner
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#a7f3d0"));
        QPolygonF fold;
        fold << QPointF(64, 12) << QPointF(80, 28) << QPointF(64, 28);
        p.drawPolygon(fold);

        // Table grid
        p.setPen(QPen(Qt::white, 1.4));
        p.setBrush(QColor(255, 255, 255, 30));
        p.drawRoundedRect(QRectF(26, 38, 44, 30), 3, 3);
        p.drawLine(26, 48, 70, 48);
        p.drawLine(26, 58, 70, 58);
        p.drawLine(44, 38, 44, 68);
        break;
    }
    case Type::Text:
    case Type::Document:
    default: {
        // Document sheet with folded corner and clean text lines
        QRectF card(16, 12, 64, 72);
        QLinearGradient grad(0, 12, 0, 84);
        grad.setColorAt(0.0, QColor("#262a33"));
        grad.setColorAt(1.0, QColor("#1e2229"));
        p.setPen(QPen(QColor("#3b4252"), 1.5));
        p.setBrush(grad);
        p.drawRoundedRect(card, 8, 8);

        // Folded top-right corner
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#64748b"));
        QPolygonF fold;
        fold << QPointF(64, 12) << QPointF(80, 28) << QPointF(64, 28);
        p.drawPolygon(fold);

        // Text lines
        p.setPen(QPen(QColor("#38bdf8"), 2.4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(28, 38, 46, 38);

        p.setPen(QPen(QColor("#94a3b8"), 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(28, 48, 64, 48);
        p.drawLine(28, 56, 60, 56);
        p.drawLine(28, 64, 44, 64);
        break;
    }
    }

    p.end();
    return QIcon(pix);
}

inline Type detectType(const QString &fileName, bool isDir) {
    if (isDir) return Type::Folder;
    QString ext = QFileInfo(fileName).suffix().toLower();
    static const QStringList codeExts = {
        "py", "cpp", "c", "h", "hpp", "cc", "cxx", "js", "ts", "html", "htm",
        "css", "scss", "json", "xml", "sh", "bash", "php", "rb", "rs", "go",
        "java", "cs", "lua", "yaml", "yml", "sql", "cmake", "ui", "qml"
    };
    if (codeExts.contains(ext)) return Type::Code;
    static const QStringList textExts = {
        "txt", "md", "markdown", "log", "cfg", "ini", "conf", "diff", "patch"
    };
    if (textExts.contains(ext)) return Type::Text;
    static const QStringList zipExts = {
        "zip", "tar", "gz", "bz2", "xz", "7z", "rar", "zst", "tgz", "iso", "deb", "rpm"
    };
    if (zipExts.contains(ext)) return Type::Zip;
    static const QStringList imageExts = {
        "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "ico", "tiff", "kra", "psd", "xcf"
    };
    if (imageExts.contains(ext)) return Type::Image;
    static const QStringList mediaExts = {
        "mp3", "wav", "flac", "ogg", "m4a", "aac", "opus", "mp4", "mkv", "avi", "mov", "webm"
    };
    if (mediaExts.contains(ext)) return Type::Media;
    if (ext == "pdf") return Type::Pdf;
    static const QStringList officeExts = {
        "docx", "doc", "xlsx", "xls", "pptx", "ppt", "odt", "ods", "odp", "rtf", "csv"
    };
    if (officeExts.contains(ext)) return Type::Office;
    return Type::Document;
}

/**
 * @brief UI control and metadata vector icon identifiers.
 */
enum class UiIcon {
    User,
    Disc,
    Calendar,
    Signal,
    File,
    Shuffle,
    Previous,
    Play,
    Pause,
    Next,
    Repeat,
    AutoAdvance,
    Volume,
    VolumeMute,
    Equalizer,
    Scissors,
    Tag
};

/**
 * @brief Generates high-DPI crisp vector pixmaps for UI buttons and cards.
 * @param icon The requested UiIcon type.
 * @param color The icon stroke/fill color.
 * @param size Icon width and height in pixels.
 * @return QIcon containing the rendered vector graphic.
 */
inline QIcon createUiIcon(UiIcon icon, const QColor &color = QColor("#cbd5e1"), int size = 24) {
    QPixmap pix(size * 2, size * 2); // 2x for Retina/Hi-DPI sharpness
    pix.setDevicePixelRatio(2.0);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Scale canvas from 24x24 reference box to target size
    double scale = static_cast<double>(size) / 24.0;
    p.scale(scale, scale);

    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (icon) {
    case UiIcon::User: {
        // Head
        p.drawEllipse(QRectF(8, 3, 8, 8));
        // Body / Shoulders
        QPainterPath body;
        body.moveTo(4, 20);
        body.quadTo(4, 15, 12, 15);
        body.quadTo(20, 15, 20, 20);
        p.drawPath(body);
        break;
    }
    case UiIcon::Disc: {
        // Outer vinyl/disc circle
        p.drawEllipse(QRectF(3, 3, 18, 18));
        // Center hole
        p.drawEllipse(QRectF(10, 10, 4, 4));
        // Groove ring
        QPen subtlePen(color, 1.0, Qt::SolidLine);
        p.setPen(subtlePen);
        p.drawEllipse(QRectF(7, 7, 10, 10));
        break;
    }
    case UiIcon::Calendar: {
        // Outline box
        p.drawRoundedRect(QRectF(3, 5, 18, 15), 2.5, 2.5);
        // Header line
        p.drawLine(QPointF(3, 10), QPointF(21, 10));
        // Top binding rings
        p.drawLine(QPointF(7, 2.5), QPointF(7, 5.5));
        p.drawLine(QPointF(17, 2.5), QPointF(17, 5.5));
        // Calendar days (small dots)
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(7, 13, 1.8, 1.8));
        p.drawEllipse(QRectF(11, 13, 1.8, 1.8));
        p.drawEllipse(QRectF(15, 13, 1.8, 1.8));
        p.drawEllipse(QRectF(7, 16.5, 1.8, 1.8));
        p.drawEllipse(QRectF(11, 16.5, 1.8, 1.8));
        p.drawEllipse(QRectF(15, 16.5, 1.8, 1.8));
        break;
    }
    case UiIcon::Signal: {
        // Central transmitter dot
        p.setBrush(color);
        p.drawEllipse(QRectF(10.5, 13.5, 3, 3));
        p.setBrush(Qt::NoBrush);
        // Waves
        p.drawArc(QRectF(7, 10, 10, 10), 45 * 16, 90 * 16);
        p.drawArc(QRectF(4, 7, 16, 16), 45 * 16, 90 * 16);
        p.drawArc(QRectF(1, 4, 22, 22), 45 * 16, 90 * 16);
        break;
    }
    case UiIcon::File: {
        // Document sheet with folded corner
        QPainterPath doc;
        doc.moveTo(5, 3);
        doc.lineTo(14, 3);
        doc.lineTo(19, 8);
        doc.lineTo(19, 21);
        doc.lineTo(5, 21);
        doc.closeSubpath();
        p.drawPath(doc);
        p.drawLine(QPointF(14, 3), QPointF(14, 8));
        p.drawLine(QPointF(14, 8), QPointF(19, 8));
        break;
    }
    case UiIcon::Shuffle: {
        QPainterPath arrow1;
        arrow1.moveTo(16, 4); arrow1.lineTo(20, 4); arrow1.lineTo(20, 8);
        p.drawPath(arrow1);
        p.drawLine(QPointF(4, 20), QPointF(9, 15));
        p.drawLine(QPointF(9, 15), QPointF(15, 9));
        p.drawLine(QPointF(15, 9), QPointF(20, 4));

        QPainterPath arrow2;
        arrow2.moveTo(20, 16); arrow2.lineTo(20, 20); arrow2.lineTo(16, 20);
        p.drawPath(arrow2);
        p.drawLine(QPointF(4, 4), QPointF(9, 9));
        p.drawLine(QPointF(15, 15), QPointF(20, 20));
        break;
    }
    case UiIcon::Previous: {
        // Bar
        p.drawLine(QPointF(6, 6), QPointF(6, 18));
        // Triangle
        QPainterPath tri;
        tri.moveTo(18, 6);
        tri.lineTo(8, 12);
        tri.lineTo(18, 18);
        tri.closeSubpath();
        p.setBrush(color);
        p.drawPath(tri);
        break;
    }
    case UiIcon::Next: {
        // Triangle
        QPainterPath tri;
        tri.moveTo(6, 6);
        tri.lineTo(16, 12);
        tri.lineTo(6, 18);
        tri.closeSubpath();
        p.setBrush(color);
        p.drawPath(tri);
        // Bar
        p.drawLine(QPointF(18, 6), QPointF(18, 18));
        break;
    }
    case UiIcon::Play: {
        QPainterPath tri;
        tri.moveTo(8, 5);
        tri.lineTo(18.5, 12);
        tri.lineTo(8, 19);
        tri.closeSubpath();
        p.setBrush(color);
        p.drawPath(tri);
        break;
    }
    case UiIcon::Pause: {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(7, 5, 3.5, 14), 1.5, 1.5);
        p.drawRoundedRect(QRectF(13.5, 5, 3.5, 14), 1.5, 1.5);
        break;
    }
    case UiIcon::Repeat: {
        QPainterPath topArrow;
        topArrow.moveTo(17, 2.5); topArrow.lineTo(20.5, 6); topArrow.lineTo(17, 9.5);
        p.drawPath(topArrow);
        QPainterPath path1;
        path1.moveTo(3.5, 11);
        path1.lineTo(3.5, 7.5);
        path1.quadTo(3.5, 6, 5, 6);
        path1.lineTo(20.5, 6);
        p.drawPath(path1);

        QPainterPath bottomArrow;
        bottomArrow.moveTo(7, 14.5); bottomArrow.lineTo(3.5, 18); bottomArrow.lineTo(7, 21.5);
        p.drawPath(bottomArrow);
        QPainterPath path2;
        path2.moveTo(20.5, 13);
        path2.lineTo(20.5, 16.5);
        path2.quadTo(20.5, 18, 19, 18);
        path2.lineTo(3.5, 18);
        p.drawPath(path2);
        break;
    }
    case UiIcon::AutoAdvance: {
        // Continuous playback / Playlist auto-advance (3 list lines + right play triangle)
        p.drawLine(QPointF(3.5, 6), QPointF(14.5, 6));
        p.drawLine(QPointF(3.5, 12), QPointF(12.5, 12));
        p.drawLine(QPointF(3.5, 18), QPointF(14.5, 18));
        QPainterPath tri;
        tri.moveTo(16, 8.5);
        tri.lineTo(21.5, 12);
        tri.lineTo(16, 15.5);
        tri.closeSubpath();
        p.setBrush(color);
        p.drawPath(tri);
        break;
    }
    case UiIcon::Volume: {
        // Speaker cone
        QPainterPath cone;
        cone.moveTo(4, 9);
        cone.lineTo(8, 9);
        cone.lineTo(13, 5);
        cone.lineTo(13, 19);
        cone.lineTo(8, 15);
        cone.lineTo(4, 15);
        cone.closeSubpath();
        p.setBrush(color);
        p.drawPath(cone);
        p.setBrush(Qt::NoBrush);
        // Soundwaves
        p.drawArc(QRectF(11, 8, 7, 8), -55 * 16, 110 * 16);
        p.drawArc(QRectF(9, 5, 12, 14), -55 * 16, 110 * 16);
        break;
    }
    case UiIcon::VolumeMute: {
        QPainterPath cone;
        cone.moveTo(4, 9);
        cone.lineTo(8, 9);
        cone.lineTo(13, 5);
        cone.lineTo(13, 19);
        cone.lineTo(8, 15);
        cone.lineTo(4, 15);
        cone.closeSubpath();
        p.setBrush(color);
        p.drawPath(cone);
        p.setBrush(Qt::NoBrush);
        // Cross
        p.drawLine(QPointF(16, 9), QPointF(21, 15));
        p.drawLine(QPointF(21, 9), QPointF(16, 15));
        break;
    }
    case UiIcon::Equalizer: {
        p.drawLine(QPointF(6, 4), QPointF(6, 20));
        p.drawLine(QPointF(12, 4), QPointF(12, 20));
        p.drawLine(QPointF(18, 4), QPointF(18, 20));
        p.setBrush(color);
        p.drawEllipse(QRectF(4.5, 8, 3, 3));
        p.drawEllipse(QRectF(10.5, 13, 3, 3));
        p.drawEllipse(QRectF(16.5, 7, 3, 3));
        break;
    }
    case UiIcon::Scissors: {
        p.drawEllipse(QRectF(4, 4, 5, 5));
        p.drawEllipse(QRectF(4, 15, 5, 5));
        p.drawLine(QPointF(8.5, 7.5), QPointF(19, 17));
        p.drawLine(QPointF(8.5, 16.5), QPointF(19, 7));
        break;
    }
    case UiIcon::Tag: {
        QPainterPath tag;
        tag.moveTo(4, 12);
        tag.lineTo(12, 4);
        tag.lineTo(19, 4);
        tag.lineTo(19, 11);
        tag.lineTo(11, 19);
        tag.closeSubpath();
        p.drawPath(tag);
        p.drawEllipse(QRectF(15, 6, 2, 2));
        break;
    }
    }

    p.end();
    return QIcon(pix);
}

} // namespace VectorIcons
