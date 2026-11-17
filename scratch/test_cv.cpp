#include <QVector>
#include <QPoint>
#include <cmath>
#include <iostream>

void analyze(const char *name, const QVector<QPoint> &points) {
    double cx = 0.0, cy = 0.0;
    for (const auto &p : points) { cx += p.x(); cy += p.y(); }
    cx /= points.size(); cy /= points.size();
    double meanRad = 0.0;
    for (const auto &p : points) meanRad += std::hypot(p.x() - cx, p.y() - cy);
    meanRad /= points.size();
    double varRad = 0.0;
    for (const auto &p : points) {
        double d = std::hypot(p.x() - cx, p.y() - cy) - meanRad;
        varRad += d * d;
    }
    varRad /= points.size();
    double cv = std::sqrt(varRad) / meanRad;
    std::cout << name << ": cv=" << cv << " meanRad=" << meanRad << "\n";
}

int main() {
    QVector<QPoint> circlePoints;
    for (int deg = 0; deg <= 360; deg += 10) {
        double rad = deg * M_PI / 180.0;
        circlePoints.append(QPoint(qRound(200 + 80 * std::cos(rad)), qRound(200 + 80 * std::sin(rad))));
    }
    analyze("Circle", circlePoints);

    QVector<QPoint> rectPoints;
    for (int x = 100; x <= 250; x += 10) rectPoints.append(QPoint(x, 100));
    for (int y = 100; y <= 200; y += 10) rectPoints.append(QPoint(250, y));
    for (int x = 250; x >= 100; x -= 10) rectPoints.append(QPoint(x, 200));
    for (int y = 200; y >= 100; y -= 10) rectPoints.append(QPoint(100, y));
    analyze("Rectangle", rectPoints);
    return 0;
}
