#pragma once

#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>
#include <QStringList>
#include <cmath>

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
        // Sleek modern macOS/GNOME-inspired rounded folder with smooth curves and subtle depth
        QColor backTop = customColor.isValid() ? customColor.darker(118) : QColor("#0284c7");
        QColor backBottom = customColor.isValid() ? customColor.darker(145) : QColor("#0369a1");
        QColor frontTop = customColor.isValid() ? customColor.lighter(110) : QColor("#38bdf8");
        QColor frontBottom = customColor.isValid() ? customColor : QColor("#0ea5e9");

        // 1. Soft drop shadow beneath folder body
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 38));
        p.drawRoundedRect(QRectF(11, 26, 74, 57), 11, 11);

        // 2. Unified Back Plate with smooth curved top-left tab
        QPainterPath backPath;
        backPath.moveTo(10, 28);
        backPath.quadTo(10, 17, 21, 17);
        backPath.lineTo(37, 17);
        backPath.quadTo(43, 17, 46, 22);
        backPath.quadTo(49, 25, 54, 25);
        backPath.lineTo(75, 25);
        backPath.quadTo(86, 25, 86, 36);
        backPath.lineTo(86, 70);
        backPath.quadTo(86, 80, 76, 80);
        backPath.lineTo(20, 80);
        backPath.quadTo(10, 80, 10, 70);
        backPath.closeSubpath();

        QLinearGradient backGrad(0, 17, 0, 80);
        backGrad.setColorAt(0.0, backTop);
        backGrad.setColorAt(1.0, backBottom);
        p.setBrush(backGrad);
        p.drawPath(backPath);

        // 3. Subtle recessed inner paper sheet (tucked cleanly below the back plate rim)
        p.setBrush(QColor(255, 255, 255, 230));
        p.drawRoundedRect(QRectF(17, 28, 62, 16), 5, 5);

        // 4. Front Folder Pocket (smooth squircle with rich vertical gradient)
        QLinearGradient flapGrad(0, 33, 0, 81);
        flapGrad.setColorAt(0.0, frontTop);
        flapGrad.setColorAt(1.0, frontBottom);
        p.setBrush(flapGrad);
        p.drawRoundedRect(QRectF(10, 33, 76, 48), 10, 10);

        // Subtle specular highlight along the top curve of the front pocket
        QPainterPath highlightPath;
        highlightPath.moveTo(14, 34.0);
        highlightPath.lineTo(82, 34.0);
        p.setPen(QPen(QColor(255, 255, 255, 105), 1.2, Qt::SolidLine, Qt::RoundCap));
        p.drawPath(highlightPath);

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
            p.drawRoundedRect(QRectF(36, 48, 24, 16), 3, 3);
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
    Tag,
    Share,
    Settings,
    Edit,
    Check,
    ExternalLink,
    ChevronLeft,
    ChevronRight,
    Grid,
    Info,
    Search,
    Close,
    RotateLeft,
    RotateRight,
    FlipH,
    FlipV,
    Crop,
    Palette,
    Highlighter,
    Pen,
    Typewriter,
    Signature,
    Shapes,
    Stamp,
    Extract,
    Plus,
    Trash,
    Test,
    Numbers,
    Wrap,
    Eye,
    Playlist,
    Music,
    Repair,
    Save,
    Zoom
};

/**
 * @brief Generates high-DPI crisp vector pixmaps for UI buttons and cards.
 * @param icon The requested UiIcon type.
 * @param color The icon stroke/fill color.
 * @param size Icon width and height in pixels.
 * @return QIcon containing the rendered vector graphic.
 */
inline QPixmap renderUiPixmap(UiIcon icon, const QColor &color, int targetSize, qreal dpr) {
    int physicalSize = qRound(targetSize * dpr);
    QPixmap pix(physicalSize, physicalSize);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Scale canvas from 24x24 reference box to target size
    double scale = static_cast<double>(targetSize) / 24.0;
    p.scale(scale, scale);

    QPen pen(color, 2.15, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
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
    case UiIcon::Share: {
        // macOS / Lucide styled share tray & arrow (scaled to fill 19-20px)
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath tray;
        tray.moveTo(2.5, 11.5);
        tray.lineTo(2.5, 21.0);
        tray.quadTo(2.5, 22.5, 4.0, 22.5);
        tray.lineTo(20.0, 22.5);
        tray.quadTo(21.5, 22.5, 21.5, 21.0);
        tray.lineTo(21.5, 11.5);
        p.drawPath(tray);

        // Arrow pointing upward
        p.drawLine(QPointF(12.0, 16.0), QPointF(12.0, 2.0));
        QPainterPath arrow;
        arrow.moveTo(6.0, 7.5);
        arrow.lineTo(12.0, 2.0);
        arrow.lineTo(18.0, 7.5);
        p.drawPath(arrow);
        break;
    }
    case UiIcon::Settings: {
        // Gear / cog with center hole (bold 20px)
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawEllipse(QRectF(8.0, 8.0, 8.0, 8.0));
        p.drawEllipse(QRectF(4.0, 4.0, 16.0, 16.0));
        for (int i = 0; i < 8; ++i) {
            double angle = i * (3.14159265358979323846 / 4.0);
            double c = std::cos(angle);
            double s = std::sin(angle);
            p.drawLine(QPointF(12.0 + 7.0 * c, 12.0 + 7.0 * s), QPointF(12.0 + 11.2 * c, 12.0 + 11.2 * s));
        }
        break;
    }
    case UiIcon::Edit: {
        // Modern pencil icon
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath pencil;
        pencil.moveTo(17.5, 2.5);
        pencil.lineTo(21.5, 6.5);
        pencil.lineTo(7.5, 20.5);
        pencil.lineTo(2.5, 20.5);
        pencil.lineTo(2.5, 15.5);
        pencil.closeSubpath();
        p.drawPath(pencil);
        p.drawLine(QPointF(14.0, 6.0), QPointF(18.0, 10.0));
        break;
    }
    case UiIcon::Check: {
        // Crisp checkmark
        p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath check;
        check.moveTo(3.5, 12.0);
        check.lineTo(9.5, 18.5);
        check.lineTo(21.5, 5.5);
        p.drawPath(check);
        break;
    }
    case UiIcon::ExternalLink: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Diagonal arrow pointing top-right
        p.drawLine(QPointF(8.5, 15.5), QPointF(22.0, 2.0));
        p.drawLine(QPointF(15.0, 2.0), QPointF(22.0, 2.0));
        p.drawLine(QPointF(22.0, 2.0), QPointF(22.0, 9.0));

        // Canvas/window container outline
        QPainterPath box;
        box.moveTo(16.5, 9.5);
        box.lineTo(16.5, 22.0);
        box.lineTo(2.0, 22.0);
        box.lineTo(2.0, 7.5);
        box.lineTo(14.5, 7.5);
        p.drawPath(box);
        break;
    }
    case UiIcon::ChevronLeft: {
        p.setPen(QPen(color, 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        path.moveTo(16.5, 2.5);
        path.lineTo(6.5, 12.0);
        path.lineTo(16.5, 21.5);
        p.drawPath(path);
        break;
    }
    case UiIcon::ChevronRight: {
        p.setPen(QPen(color, 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        path.moveTo(7.5, 2.5);
        path.lineTo(17.5, 12.0);
        path.lineTo(7.5, 21.5);
        p.drawPath(path);
        break;
    }
    case UiIcon::Grid: {
        // macOS Quick Look 2x2 solid tile gallery icon (scaled to fill 19-20px)
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(1.5, 1.5, 9.5, 9.5), 2.0, 2.0);
        p.drawRoundedRect(QRectF(13.0, 1.5, 9.5, 9.5), 2.0, 2.0);
        p.drawRoundedRect(QRectF(1.5, 13.0, 9.5, 9.5), 2.0, 2.0);
        p.drawRoundedRect(QRectF(13.0, 13.0, 9.5, 9.5), 2.0, 2.0);
        break;
    }
    case UiIcon::Info: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(1.5, 1.5, 21.0, 21.0));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QRectF(10.5, 5.2, 3.0, 3.0));
        p.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(12.0, 10.0), QPointF(12.0, 17.5));
        break;
    }
    case UiIcon::Search: {
        p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(2.0, 2.0, 14.0, 14.0));
        p.drawLine(QPointF(12.8, 12.8), QPointF(22.2, 22.2));
        break;
    }
    case UiIcon::Zoom: {
        p.setPen(QPen(color, 2.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(2.0, 2.0, 13.5, 13.5));
        p.drawLine(QPointF(12.5, 12.5), QPointF(21.5, 21.5));
        // Centered plus sign inside the magnifying glass
        p.drawLine(QPointF(8.75, 5.5), QPointF(8.75, 12.0));
        p.drawLine(QPointF(5.5, 8.75), QPointF(12.0, 8.75));
        break;
    }
    case UiIcon::Close: {
        p.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(3.5, 3.5), QPointF(20.5, 20.5));
        p.drawLine(QPointF(20.5, 3.5), QPointF(3.5, 20.5));
        break;
    }
    case UiIcon::RotateLeft: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Circular arc counter-clockwise
        QRectF arcRect(4.0, 4.0, 16.0, 16.0);
        p.drawArc(arcRect, 45 * 16, 260 * 16);
        // Arrow head at top-left
        QPainterPath arrow;
        arrow.moveTo(3.0, 9.0);
        arrow.lineTo(4.5, 4.0);
        arrow.lineTo(9.5, 5.5);
        p.drawPath(arrow);
        break;
    }
    case UiIcon::RotateRight: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Circular arc clockwise
        QRectF arcRect(4.0, 4.0, 16.0, 16.0);
        p.drawArc(arcRect, 135 * 16, -260 * 16);
        // Arrow head at top-right
        QPainterPath arrow;
        arrow.moveTo(21.0, 9.0);
        arrow.lineTo(19.5, 4.0);
        arrow.lineTo(14.5, 5.5);
        p.drawPath(arrow);
        break;
    }
    case UiIcon::FlipH: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Left triangle
        QPainterPath leftTri;
        leftTri.moveTo(9.5, 5.0);
        leftTri.lineTo(2.5, 12.0);
        leftTri.lineTo(9.5, 19.0);
        leftTri.closeSubpath();
        p.drawPath(leftTri);
        // Right triangle (filled)
        QPainterPath rightTri;
        rightTri.moveTo(14.5, 5.0);
        rightTri.lineTo(21.5, 12.0);
        rightTri.lineTo(14.5, 19.0);
        rightTri.closeSubpath();
        p.setBrush(color);
        p.drawPath(rightTri);
        p.setBrush(Qt::NoBrush);
        // Vertical dashed divider
        p.setPen(QPen(color, 1.4, Qt::DashLine, Qt::RoundCap));
        p.drawLine(QPointF(12.0, 3.0), QPointF(12.0, 21.0));
        break;
    }
    case UiIcon::FlipV: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Top triangle
        QPainterPath topTri;
        topTri.moveTo(5.0, 9.5);
        topTri.lineTo(12.0, 2.5);
        topTri.lineTo(19.0, 9.5);
        topTri.closeSubpath();
        p.drawPath(topTri);
        // Bottom triangle (filled)
        QPainterPath botTri;
        botTri.moveTo(5.0, 14.5);
        botTri.lineTo(12.0, 21.5);
        botTri.lineTo(19.0, 14.5);
        botTri.closeSubpath();
        p.setBrush(color);
        p.drawPath(botTri);
        p.setBrush(Qt::NoBrush);
        // Horizontal dashed divider
        p.setPen(QPen(color, 1.4, Qt::DashLine, Qt::RoundCap));
        p.drawLine(QPointF(3.0, 12.0), QPointF(21.0, 12.0));
        break;
    }
    case UiIcon::Crop: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Top-left L
        QPainterPath tl;
        tl.moveTo(6.0, 2.0);
        tl.lineTo(6.0, 18.0);
        tl.lineTo(22.0, 18.0);
        p.drawPath(tl);
        // Bottom-right L
        QPainterPath br;
        br.moveTo(18.0, 22.0);
        br.lineTo(18.0, 6.0);
        br.lineTo(2.0, 6.0);
        p.drawPath(br);
        break;
    }
    case UiIcon::Palette: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath pal;
        pal.moveTo(12.0, 3.0);
        pal.cubicTo(19.0, 3.0, 21.5, 8.0, 21.5, 13.0);
        pal.cubicTo(21.5, 17.5, 18.5, 21.0, 14.5, 21.0);
        pal.cubicTo(13.0, 21.0, 12.0, 20.0, 11.5, 18.5);
        pal.cubicTo(11.0, 17.0, 9.5, 16.0, 8.0, 16.0);
        pal.cubicTo(6.0, 16.0, 2.5, 17.5, 2.5, 13.0);
        pal.cubicTo(2.5, 7.5, 7.0, 3.0, 12.0, 3.0);
        pal.closeSubpath();
        p.drawPath(pal);
        // Dots
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QRectF(7.0, 6.5, 2.5, 2.5));
        p.drawEllipse(QRectF(12.0, 5.5, 2.5, 2.5));
        p.drawEllipse(QRectF(16.5, 8.5, 2.5, 2.5));
        p.drawEllipse(QRectF(16.0, 14.0, 2.8, 2.8));
        break;
    }
    case UiIcon::Highlighter: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Angled marker body
        QPainterPath marker;
        marker.moveTo(15.0, 3.0);
        marker.lineTo(21.0, 9.0);
        marker.lineTo(12.0, 18.0);
        marker.lineTo(6.0, 12.0);
        marker.closeSubpath();
        p.drawPath(marker);
        // Chisel tip
        QPainterPath tip;
        tip.moveTo(6.0, 12.0);
        tip.lineTo(3.0, 20.0);
        tip.lineTo(7.0, 21.0);
        tip.lineTo(12.0, 18.0);
        p.drawPath(tip);
        break;
    }
    case UiIcon::Pen: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath pencil;
        pencil.moveTo(17.5, 3.5);
        pencil.lineTo(20.5, 6.5);
        pencil.lineTo(8.5, 18.5);
        pencil.lineTo(3.5, 20.5);
        pencil.lineTo(5.5, 15.5);
        pencil.closeSubpath();
        p.drawPath(pencil);
        p.drawLine(QPointF(6.0, 16.0), QPointF(8.5, 18.5));
        break;
    }
    case UiIcon::Typewriter: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(4.0, 5.0), QPointF(20.0, 5.0));
        p.drawLine(QPointF(12.0, 5.0), QPointF(12.0, 20.0));
        p.drawLine(QPointF(8.0, 20.0), QPointF(16.0, 20.0));
        break;
    }
    case UiIcon::Signature: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath sig;
        sig.moveTo(3.5, 16.0);
        sig.quadTo(6.0, 8.0, 9.0, 7.0);
        sig.quadTo(12.0, 6.0, 9.5, 14.0);
        sig.quadTo(12.0, 11.0, 15.0, 13.0);
        sig.quadTo(18.0, 15.0, 21.0, 9.0);
        p.drawPath(sig);
        p.drawLine(QPointF(3.0, 20.5), QPointF(21.0, 20.5));
        break;
    }
    case UiIcon::Shapes: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Circle top-left
        p.drawEllipse(QRectF(3.0, 3.0, 11.0, 11.0));
        // Rounded rect bottom-right
        p.drawRoundedRect(QRectF(10.0, 10.0, 11.0, 11.0), 2.5, 2.5);
        break;
    }
    case UiIcon::Stamp: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Knob top handle
        p.drawEllipse(QRectF(9.5, 3.0, 5.0, 5.0));
        // Neck
        p.drawLine(QPointF(12.0, 8.0), QPointF(12.0, 12.0));
        // Flange
        QPainterPath flange;
        flange.moveTo(7.0, 14.0);
        flange.lineTo(12.0, 12.0);
        flange.lineTo(17.0, 14.0);
        flange.lineTo(19.0, 17.0);
        flange.lineTo(5.0, 17.0);
        flange.closeSubpath();
        p.drawPath(flange);
        // Base stamp pad
        p.drawRoundedRect(QRectF(3.5, 18.0, 17.0, 3.5), 1.5, 1.5);
        break;
    }
    case UiIcon::Extract: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Open box
        QPainterPath box;
        box.moveTo(4.0, 13.0);
        box.lineTo(4.0, 20.0);
        box.lineTo(20.0, 20.0);
        box.lineTo(20.0, 13.0);
        p.drawPath(box);
        // Arrow pointing up
        p.drawLine(QPointF(12.0, 15.0), QPointF(12.0, 4.0));
        QPainterPath arr;
        arr.moveTo(7.5, 8.5);
        arr.lineTo(12.0, 4.0);
        arr.lineTo(16.5, 8.5);
        p.drawPath(arr);
        break;
    }
    case UiIcon::Plus: {
        p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(12.0, 4.5), QPointF(12.0, 19.5));
        p.drawLine(QPointF(4.5, 12.0), QPointF(19.5, 12.0));
        break;
    }
    case UiIcon::Trash: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Lid
        p.drawLine(QPointF(4.0, 6.0), QPointF(20.0, 6.0));
        p.drawLine(QPointF(9.5, 6.0), QPointF(9.5, 3.5));
        p.drawLine(QPointF(9.5, 3.5), QPointF(14.5, 3.5));
        p.drawLine(QPointF(14.5, 3.5), QPointF(14.5, 6.0));
        // Body
        QPainterPath body;
        body.moveTo(5.5, 6.0);
        body.lineTo(6.5, 20.0);
        body.lineTo(17.5, 20.0);
        body.lineTo(18.5, 6.0);
        p.drawPath(body);
        // Vertical ribs
        p.drawLine(QPointF(9.5, 9.5), QPointF(9.5, 16.5));
        p.drawLine(QPointF(14.5, 9.5), QPointF(14.5, 16.5));
        break;
    }
    case UiIcon::Test: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Shield
        QPainterPath shield;
        shield.moveTo(12.0, 3.0);
        shield.lineTo(19.0, 6.0);
        shield.lineTo(19.0, 13.0);
        shield.quadTo(19.0, 18.5, 12.0, 21.5);
        shield.quadTo(5.0, 18.5, 5.0, 13.0);
        shield.lineTo(5.0, 6.0);
        shield.closeSubpath();
        p.drawPath(shield);
        // Inner checkmark
        QPainterPath check;
        check.moveTo(8.5, 12.0);
        check.lineTo(11.0, 14.5);
        check.lineTo(15.5, 9.5);
        p.drawPath(check);
        break;
    }
    case UiIcon::Numbers: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Hash #
        p.drawLine(QPointF(8.5, 4.0), QPointF(6.5, 20.0));
        p.drawLine(QPointF(16.5, 4.0), QPointF(14.5, 20.0));
        p.drawLine(QPointF(4.0, 9.0), QPointF(19.0, 9.0));
        p.drawLine(QPointF(3.0, 15.0), QPointF(18.0, 15.0));
        break;
    }
    case UiIcon::Wrap: {
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath wrapPath;
        wrapPath.moveTo(4.0, 7.0);
        wrapPath.lineTo(15.0, 7.0);
        wrapPath.quadTo(20.0, 7.0, 20.0, 12.0);
        wrapPath.quadTo(20.0, 17.0, 15.0, 17.0);
        wrapPath.lineTo(7.0, 17.0);
        p.drawPath(wrapPath);
        // Arrowhead
        QPainterPath arr;
        arr.moveTo(10.5, 13.5);
        arr.lineTo(6.5, 17.0);
        arr.lineTo(10.5, 20.5);
        p.drawPath(arr);
        break;
    }
    case UiIcon::Eye: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        QPainterPath eye;
        eye.moveTo(2.5, 12.0);
        eye.quadTo(12.0, 4.0, 21.5, 12.0);
        eye.quadTo(12.0, 20.0, 2.5, 12.0);
        p.drawPath(eye);
        p.setBrush(color);
        p.drawEllipse(QRectF(9.5, 9.5, 5.0, 5.0));
        break;
    }
    case UiIcon::Playlist: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(4.0, 6.5), QPointF(20.0, 6.5));
        p.drawLine(QPointF(4.0, 12.0), QPointF(20.0, 12.0));
        p.drawLine(QPointF(4.0, 17.5), QPointF(14.0, 17.5));
        break;
    }
    case UiIcon::Music: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(color);
        p.drawEllipse(QRectF(3.5, 13.5, 6.5, 5.5));
        p.drawLine(QPointF(10.0, 15.0), QPointF(10.0, 5.0));
        p.drawLine(QPointF(10.0, 5.0), QPointF(18.5, 8.0));
        p.drawLine(QPointF(18.5, 8.0), QPointF(18.5, 12.5));
        p.drawEllipse(QRectF(12.0, 11.0, 6.5, 5.5));
        break;
    }
    case UiIcon::Repair: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        // Open-ended wrench angled at 45 degrees
        QPainterPath wrench;
        wrench.moveTo(14.5, 3.5);
        wrench.cubicTo(17.5, 2.5, 20.5, 4.0, 21.5, 7.0);
        wrench.lineTo(18.0, 8.0);
        wrench.lineTo(17.0, 11.0);
        wrench.lineTo(20.0, 12.5);
        wrench.cubicTo(19.0, 15.5, 16.0, 17.0, 13.0, 16.0);
        wrench.lineTo(5.5, 21.5);
        wrench.cubicTo(4.2, 22.4, 2.5, 22.0, 1.8, 20.7);
        wrench.cubicTo(1.1, 19.4, 1.5, 17.7, 2.8, 16.8);
        wrench.lineTo(10.5, 11.0);
        wrench.cubicTo(10.0, 8.0, 11.5, 4.5, 14.5, 3.5);
        wrench.closeSubpath();
        p.drawPath(wrench);
        break;
    }
    case UiIcon::Save: {
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        // Floppy disk outer outline with chamfered top-right corner
        QPainterPath disk;
        disk.moveTo(4.0, 20.0);
        disk.lineTo(4.0, 4.0);
        disk.lineTo(16.5, 4.0);
        disk.lineTo(20.0, 7.5);
        disk.lineTo(20.0, 20.0);
        disk.closeSubpath();
        p.drawPath(disk);
        // Top shutter window
        p.drawRect(QRectF(8.0, 4.0, 7.5, 5.0));
        // Bottom label area
        p.drawRoundedRect(QRectF(7.0, 13.0, 10.0, 7.0), 1.0, 1.0);
        break;
    }
    }

    p.end();
    return pix;
}

/**
 * @brief Renders a resolution-independent crisp vector UI icon supporting both 1x and HiDPI 2x.
 * @param icon The requested UiIcon enum type.
 * @param color The icon stroke/fill color.
 * @param size Icon width and height in pixels.
 * @return QIcon containing 1x and 2x rendered vector graphics.
 */
inline QIcon createUiIcon(UiIcon icon, const QColor &color = QColor("#cbd5e1"), int size = 24) {
    QIcon qicon;
    qicon.addPixmap(renderUiPixmap(icon, color, size, 1.0));
    qicon.addPixmap(renderUiPixmap(icon, color, size, 1.5));
    qicon.addPixmap(renderUiPixmap(icon, color, size, 1.75));
    qicon.addPixmap(renderUiPixmap(icon, color, size, 2.0));
    qicon.addPixmap(renderUiPixmap(icon, color, size, 3.0));
    return qicon;
}

} // namespace VectorIcons
