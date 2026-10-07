#pragma once

#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>
#include <QStringList>

namespace VectorIcons {

enum class Type {
    Folder,
    Code,
    Zip,
    Image,
    Media,
    Document,
    Pdf,
    Office,
    Text
};

inline QIcon createIcon(Type type, const QColor &customColor = QColor()) {
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(customColor.isValid() ? customColor : QColor("#9da4af"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);

    switch (type) {
    case Type::Folder: {
        // Vibrant modern folder with tab and body
        QColor tabColor = customColor.isValid() ? customColor : QColor("#38bdf8");
        QColor bodyColor = customColor.isValid() ? customColor.lighter(110) : QColor("#0284c7");

        // Tab
        QPainterPath tab;
        tab.moveTo(6, 20);
        tab.lineTo(6, 15);
        tab.quadTo(6, 12, 10, 12);
        tab.lineTo(26, 12);
        tab.quadTo(30, 12, 33, 16);
        tab.lineTo(36, 20);
        tab.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(tabColor);
        p.drawPath(tab);

        // Back folder body
        p.setBrush(QColor("#0369a1"));
        p.drawRoundedRect(6, 18, 52, 38, 5, 5);

        // Front folder body (folded front sheet)
        p.setBrush(bodyColor);
        QPainterPath front;
        front.moveTo(6, 25);
        front.lineTo(58, 25);
        front.lineTo(54, 56);
        front.quadTo(53, 58, 50, 58);
        front.lineTo(14, 58);
        front.quadTo(11, 58, 10, 56);
        front.lineTo(6, 25);
        p.drawPath(front);
        break;
    }
    case Type::Zip: {
        // Vibrant modern archive package/folder in warm amber with zipper
        QColor tabColor = customColor.isValid() ? customColor : QColor("#f59e0b");
        QColor bodyColor = customColor.isValid() ? customColor.lighter(110) : QColor("#d97706");
        QColor backColor = QColor("#b45309");

        // Tab
        QPainterPath tab;
        tab.moveTo(6, 20);
        tab.lineTo(6, 15);
        tab.quadTo(6, 12, 10, 12);
        tab.lineTo(26, 12);
        tab.quadTo(30, 12, 33, 16);
        tab.lineTo(36, 20);
        tab.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(tabColor);
        p.drawPath(tab);

        // Back folder body
        p.setBrush(backColor);
        p.drawRoundedRect(6, 18, 52, 38, 5, 5);

        // Front folder body
        p.setBrush(bodyColor);
        QPainterPath front;
        front.moveTo(6, 25);
        front.lineTo(58, 25);
        front.lineTo(54, 56);
        front.quadTo(53, 58, 50, 58);
        front.lineTo(14, 58);
        front.quadTo(11, 58, 10, 56);
        front.lineTo(6, 25);
        p.drawPath(front);

        // Vertical zipper band down center
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#334155"));
        p.drawRoundedRect(QRectF(29, 18, 6, 40), 1.5, 1.5);

        // Zipper teeth (alternating gold/silver marks)
        p.setPen(QPen(QColor("#fbbf24"), 1.2));
        for (int y = 20; y <= 54; y += 4) {
            p.drawLine(29, y, 32, y);
            p.drawLine(32, y + 2, 35, y + 2);
        }

        // Zipper pull slider box
        p.setPen(QPen(QColor("#475569"), 1.0));
        p.setBrush(QColor("#cbd5e1"));
        p.drawRoundedRect(QRectF(28, 34, 8, 8), 1.5, 1.5);

        // Zipper pull tab
        QPainterPath pull;
        pull.moveTo(30, 42);
        pull.lineTo(34, 42);
        pull.lineTo(35, 51);
        pull.quadTo(32, 53, 29, 51);
        pull.closeSubpath();
        p.setBrush(QColor("#94a3b8"));
        p.drawPath(pull);
        p.setBrush(QColor("#334155"));
        p.drawEllipse(QRectF(31, 46, 2, 2));
        break;
    }
    default: {
        // Scaled drawing from 24x24 reference box to 64x64 canvas
        p.save();
        double scale = 64.0 / 24.0;
        p.scale(scale, scale);
        QPen scaledPen(customColor.isValid() ? customColor : QColor("#9da4af"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(scaledPen);

        switch (type) {
        case Type::Code: {
            // <
            QPainterPath p1;
            p1.moveTo(8, 8);
            p1.lineTo(4, 12);
            p1.lineTo(8, 16);
            p.strokePath(p1, scaledPen);
            // /
            p.drawLine(14, 7, 10, 17);
            // >
            QPainterPath p2;
            p2.moveTo(16, 8);
            p2.lineTo(20, 12);
            p2.lineTo(16, 16);
            p.strokePath(p2, scaledPen);
            break;
        }
        case Type::Image: {
            QPainterPath path;
            path.addRoundedRect(QRectF(4, 5, 16, 14), 2, 2);
            p.strokePath(path, scaledPen);
            p.drawEllipse(7, 8, 2, 2);
            QPainterPath m;
            m.moveTo(6, 17);
            m.lineTo(10, 12);
            m.lineTo(13, 15);
            m.lineTo(16, 11);
            m.lineTo(18, 15);
            p.strokePath(m, scaledPen);
            break;
        }
        case Type::Media: {
            p.drawEllipse(6, 13, 5, 4);
            p.drawLine(11, 15, 11, 6);
            p.drawLine(11, 6, 17, 4);
            p.drawLine(17, 4, 17, 13);
            p.drawEllipse(12, 11, 5, 4);
            break;
        }
        case Type::Pdf: {
            QPainterPath path;
            path.moveTo(5, 4);
            path.lineTo(14, 4);
            path.lineTo(19, 9);
            path.lineTo(19, 20);
            path.lineTo(5, 20);
            path.closeSubpath();
            p.strokePath(path, scaledPen);
            p.drawLine(14, 4, 14, 9);
            p.drawLine(14, 9, 19, 9);
            QFont f = p.font();
            f.setPixelSize(8);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRect(5, 11, 14, 8), Qt::AlignCenter, "PDF");
            break;
        }
        case Type::Office: {
            QPainterPath path;
            path.moveTo(5, 4);
            path.lineTo(14, 4);
            path.lineTo(19, 9);
            path.lineTo(19, 20);
            path.lineTo(5, 20);
            path.closeSubpath();
            p.strokePath(path, scaledPen);
            p.drawLine(14, 4, 14, 9);
            p.drawLine(14, 9, 19, 9);
            p.drawLine(8, 12, 16, 12);
            p.drawLine(8, 15, 16, 15);
            p.drawLine(8, 18, 13, 18);
            break;
        }
        case Type::Text:
        case Type::Document:
        default: {
            QPainterPath path;
            path.moveTo(5, 4);
            path.lineTo(14, 4);
            path.lineTo(19, 9);
            path.lineTo(19, 20);
            path.lineTo(5, 20);
            path.closeSubpath();
            p.strokePath(path, scaledPen);
            p.drawLine(14, 4, 14, 9);
            p.drawLine(14, 9, 19, 9);
            p.drawLine(8, 13, 16, 13);
            p.drawLine(8, 16, 14, 16);
            break;
        }
        }
        p.restore();
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
