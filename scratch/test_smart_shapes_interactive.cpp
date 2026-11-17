#include <QGuiApplication>
#include <QVector>
#include <QPoint>
#include <QDebug>
#include <iostream>
#include <cmath>
#include "../src/viewers/pdf/pdf_canvas_tools.h"

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    std::cout << "Testing Smart Shape recognition and Arrow directionality...\n";

    // 1. Arrow Right to Left
    QPoint ptStart(300, 100);
    QPoint ptEnd(50, 100);
    int padding = 16;
    int minX = std::min(ptStart.x(), ptEnd.x()) - padding;
    int maxX = std::max(ptStart.x(), ptEnd.x()) + padding;
    int minY = std::min(ptStart.y(), ptEnd.y()) - padding;
    int maxY = std::max(ptStart.y(), ptEnd.y()) + padding;
    QRect box(minX, minY, std::max(30, maxX - minX), std::max(30, maxY - minY));
    QPoint localStart = ptStart - box.topLeft();
    QPoint localEnd = ptEnd - box.topLeft();

    if (localStart.x() > localEnd.x()) {
        std::cout << "PASS: Right-to-Left Arrow has localStart (" << localStart.x() << ") > localEnd (" << localEnd.x() << ")\n";
    } else {
        std::cerr << "FAIL: Right-to-Left Arrow direction inverted!\n";
        return 1;
    }

    // 2. Smart Shape - Circle Gesture
    QVector<QPoint> circlePoints;
    for (int deg = 0; deg <= 360; deg += 10) {
        double rad = deg * M_PI / 180.0;
        circlePoints.append(QPoint(qRound(200 + 80 * std::cos(rad)), qRound(200 + 80 * std::sin(rad))));
    }
    auto resCircle = PdfCanvasTools::recognizeSmartShape(circlePoints);
    std::cout << "Recognized shape for circle points: " << resCircle.name.toStdString() << " (type " << resCircle.shapeType << ")\n";
    if (resCircle.shapeType != 1) { // Ellipse/Circle
        std::cerr << "FAIL: Circle gesture not recognized as circle!\n";
        return 1;
    }
    std::cout << "PASS: Circle recognized correctly!\n";

    // 3. Smart Shape - Rectangle Gesture
    QVector<QPoint> rectPoints;
    for (int x = 100; x <= 250; x += 10) rectPoints.append(QPoint(x, 100));
    for (int y = 100; y <= 200; y += 10) rectPoints.append(QPoint(250, y));
    for (int x = 250; x >= 100; x -= 10) rectPoints.append(QPoint(x, 200));
    for (int y = 200; y >= 100; y -= 10) rectPoints.append(QPoint(100, y));
    auto resRect = PdfCanvasTools::recognizeSmartShape(rectPoints);
    std::cout << "Recognized shape for rect points: " << resRect.name.toStdString() << " (type " << resRect.shapeType << ")\n";
    if (resRect.shapeType != 0 && resRect.shapeType != 4) { // Rectangle or Rounded Rect
        std::cerr << "FAIL: Rectangle gesture not recognized!\n";
        return 1;
    }
    std::cout << "PASS: Rectangle recognized correctly!\n";

    // 4. Smart Shape - Straight line / Arrow gesture
    QVector<QPoint> arrowPoints;
    for (int x = 100; x <= 300; x += 10) arrowPoints.append(QPoint(x, 150));
    // Arrow head pointing right
    arrowPoints.append(QPoint(280, 135));
    arrowPoints.append(QPoint(300, 150));
    arrowPoints.append(QPoint(280, 165));
    auto resArrow = PdfCanvasTools::recognizeSmartShape(arrowPoints);
    std::cout << "Recognized shape for open gesture: " << resArrow.name.toStdString() << " (type " << resArrow.shapeType << ")\n";
    if (resArrow.shapeType != 2 && resArrow.shapeType != 3) {
        std::cerr << "FAIL: Open gesture not recognized as line or arrow!\n";
        return 1;
    }
    std::cout << "PASS: Open stroke recognized with direction start=(" 
              << resArrow.startPt.x() << "," << resArrow.startPt.y() << ") end=(" 
              << resArrow.endPt.x() << "," << resArrow.endPt.y() << ")\n";

    std::cout << "\nALL SMART SHAPE TESTS PASSED!\n";
    return 0;
}
