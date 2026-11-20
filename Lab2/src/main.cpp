#include "SVG.h"
#include <iostream>
#include <sstream>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <functional>

using namespace std;
using namespace svg;

namespace {
    string askLine(const string& prompt) {
        cout << prompt;
        string text;
        if (!getline(cin, text)) throw runtime_error("Input stream closed");
        return text;
    }

    double askDouble(const string& prompt, double lower = -1e7, double upper = 1e7, bool inclusiveLower = true) {
        while (true) {
            string value = askLine(prompt);
            istringstream in(value);
            double result;
            string extra;
            if ((in >> result) && !(in >> extra) && isfinite(result) && result <= upper && (inclusiveLower ? result >= lower : result > lower)) return result;
            cout << "Error: enter a number in the allowed range.\n";
        }
    }
    int askInt(const string& prompt, int low, int high) {
        while (true) {
            string input = askLine(prompt);
            istringstream in(input);
            int value;
            string extra;
            if ((in >> value) && !(in >> extra) && value >= low && value <= high) return value;
            cout << "Error: enter an integer from " << low << " to " << high << ".\n";
        }
    }

    string askChecked(const string& prompt, const function<void(const string&)>& validate) {
        while (true) {
            string s = askLine(prompt);
            try {
                validate(s);
                return s;
            }
            catch (const exception& e) {
                cout << "Error: " << e.what() << "\n";
            }
        }
    }

    string askColor(const string& prompt) {
        return askChecked(prompt, checkColor);
    }

    string askFilename(const string& prompt) {
        return askChecked(prompt, [](const string& s) {
            if (s.empty() || s.find_first_of("<>:\"|?*\r\n") != string::npos || s.find('/') != string::npos || s.find('\\') != string::npos || s == "." || s == ".." || s.size() < 5 || s.substr(s.size()-4) != ".svg") throw invalid_argument("Enter a simple filename ending in .svg (no path)");
        }
        );
    }

    struct Canvas {
        double width, height, x, y, vw, vh;
    };
    Canvas askCanvas(bool view = false) {
        Canvas c;
        c.width = askDouble("Canvas width (px): ", 0.0, 20000, false);
        c.height = askDouble("Canvas height (px): ", 0.0, 20000, false);
        c.x = 0;
        c.y = 0;
        c.vw = c.width;
        c.vh = c.height;
        if (view) {
            c.x = askDouble("viewBox min-x: ");
            c.y = askDouble("viewBox min-y: ");
            c.vw = askDouble("viewBox width: ", 0, 20000, false);
            c.vh = askDouble("viewBox height: ", 0, 20000, false);
        }
        return c;
    }

    SVGDocumentBuilder builderFor(const Canvas& c) {
        SVGDocumentBuilder b;
        b.canvas(c.width, c.height).viewBox(c.x, c.y, c.vw, c.vh);
        return b;
    }

    void taskRect() {
        cout << "\n--- Task 1: Transformable rounded rectangle ---\n";
        Canvas c = askCanvas(true);
        double cx = askDouble("Center cx: "), cy = askDouble("Center cy: ");
        double w = askDouble("Rectangle width: ", 0, 20000, false), h = askDouble("Rectangle height: ", 0, 20000, false);
        double rx = askDouble("Corner radius rx: ", 0, w/2), ry = askDouble("Corner radius ry: ", 0, h/2);
        double a = askDouble("Rotation angle (degrees): ", -36000, 36000);
        double dx = askDouble("Translation dx: "), dy = askDouble("Translation dy: ");
        double sx = askDouble("Scale sx (nonzero, positive): ", 0, 100, false), sy = askDouble("Scale sy (nonzero, positive): ", 0, 100, false);
        string fill = askColor("Fill color: "), stroke = askColor("Stroke color: ");
        double sw = askDouble("Stroke width: ", 0, 1000);
        string filename = askFilename("Output filename (.svg): ");
        auto rect = make_unique<Rect>(cx-w/2, cy-h/2, w, h, rx, ry);
        rect->setId("transformed_rect");
        rect->setFill(fill).setStroke(stroke, sw);
        rect->transform().translate(dx, dy).rotate(a, cx, cy).scale(sx, sy);
        auto b = builderFor(c);
        b.add(move(rect));
        b.build().save(filename);
        cout << "Saved: " << filename << "\n";
    }

    void taskGroup() {
        cout << "\n--- Task 2: Nested groups (Composite) ---\n";
        Canvas c = askCanvas();
        int n = askInt("Nesting levels N (>= 2): ", 2, 15);
        double stepScale = askDouble("Scale step (e.g. 0.82, must be 0..1): ", 0, 1, false);
        double stepShift = askDouble("Nested translation step (px): ", 0, 5000);
        double dx = askDouble("Parent dx: "), dy = askDouble("Parent dy: ");
        double angle = askDouble("Parent rotation angle: ", -36000, 36000);
        string filename = askFilename("Output filename (.svg): ");
        auto root = make_unique<Group>();
        root->setId("root_group");
        root->transform().translate(dx, dy).rotate(angle, c.width/2, c.height/2);
        Group* current = root.get();
        for (int i = 0; i < n; ++i) {
            double side = min(c.width, c.height)*0.54;
            auto rect = make_unique<Rect>(c.width/2-side/2, c.height/2-side/2, side, side, side*0.08, side*0.08);
            rect->setFill(i % 2 == 0 ? "red" : "blue")
                .setStroke("white", 2);
            rect->setOpacity(0.65);
            rect->transform().rotate(i*9, c.width/2, c.height/2);
            current->add(move(rect));
            auto circle = make_unique<Circle>(c.width/2, c.height/2, side*0.29);
            circle->setFill(i%2 == 0?"#E9C46A":"#E76F51").setStroke("#FFFFFF", 2);
            circle->setOpacity(0.7);
            current->add(move(circle));
            if (i<n-1) {
                auto inner = make_unique<Group>();
                inner->transform().translate(stepShift, stepShift).scale(stepScale, stepScale);
                Group* next = inner.get();
                current->add(move(inner));
                current = next;
            }
        }
        auto b = builderFor(c);
        b.add(move(root));
        b.build().save(filename);
        cout << "Saved: " << filename << "\n";
    }

    void taskPath() {
        cout << "\n--- Task 3: Bezier path ---\n";
        Canvas c = askCanvas();
        double x0 = askDouble("Start point x0: "), y0 = askDouble("Start point y0: ");
        double x1 = askDouble("Cubic control 1 x1: "), y1 = askDouble("Cubic control 1 y1: ");
        double x2 = askDouble("Cubic control 2 x2: "), y2 = askDouble("Cubic control 2 y2: ");
        double x3 = askDouble("Cubic endpoint x3: "), y3 = askDouble("Cubic endpoint y3: ");
        double lx = askDouble("Line endpoint x: "), ly = askDouble("Line endpoint y: ");
        double qx = askDouble("Quadratic control x: "), qy = askDouble("Quadratic control y: ");
        double ex = askDouble("Quadratic endpoint x: "), ey = askDouble("Quadratic endpoint y: ");
        bool closed = askInt("Close path? (1=yes, 0=no): ", 0, 1) == 1;
        string fill = askColor("Fill color: "), stroke = askColor("Stroke color: ");
        double sw = askDouble("Stroke width: ", 0, 1000);
        string dash = askChecked("Stroke dasharray (empty or e.g. 8,4): ", [](const string& s) {
            Path p;p.setDasharray(s);
        }
        );
        string cap = askChecked("Stroke linecap (butt/round/square): ", [](const string& s) {
            Path p;p.setLinecap(s);
        }
        );
        string join = askChecked("Stroke linejoin (miter/round/bevel): ", [](const string& s) {
            Path p;p.setLinejoin(s);
        }
        );
        string filename = askFilename("Output filename (.svg): ");
        auto path = make_unique<Path>();
        path->moveTo(x0, y0).cubicTo(x1, y1, x2, y2, x3, y3).lineTo(lx, ly).quadTo(qx, qy, ex, ey);
        if (closed) path->closePath();
        path->setFill(fill).setStroke(stroke, sw);
        path->setDasharray(dash).setLinecap(cap).setLinejoin(join);
        auto b = builderFor(c);
        b.add(move(path));
        b.build().save(filename);
        cout << "Saved: " << filename << "\n";
    }

    void taskVariant1() {
        cout << "\n--- Variant 1: Multi-stop gradient and Bezier composition ---\n";
        Canvas c = askCanvas();
        string id = askChecked("Gradient ID: ", checkId);
        int mode = askInt("Direction (1=horizontal, 2=vertical, 3=diagonal, 4=custom): ", 1, 4);
        double x1 = 0, y1 = 0, x2 = 100, y2 = 0;
        if (mode == 2) {
            x2 = 0;
            y2 = 100;
        }
        else if (mode == 3) {
            y2 = 100;
        }
        else if (mode == 4) {
            while (true) {
                x1 = askDouble("Gradient x1 (%): ", -1000, 1000);
                y1 = askDouble("Gradient y1 (%): ", -1000, 1000);
                x2 = askDouble("Gradient x2 (%): ", -1000, 1000);
                y2 = askDouble("Gradient y2 (%): ", -1000, 1000);
                if (x1 != x2 || y1 != y2) break;
                cout<<"Error: gradient points must differ.\n";
            }
        }
        LinearGradient grad(id, x1, y1, x2, y2);
        int n = askInt("Number of stops (minimum 3): ", 3, 20);
        for (int i = 0; i < n; ++i) {
            while (true) {
                cout << "Stop " << i+1 << ":\n";
                double offset = askDouble("  Offset (0..1): ", 0, 1);
                string color = askColor("  Color: ");
                double opacity = askDouble("  Stop opacity (0..1): ", 0, 1);
                try {
                    grad.addStop(offset, color, opacity);
                    break;
                }
                catch (const exception& e) {
                    cout<<"Error: "<<e.what()<<". Try this stop again.\n";
                }
            }
        }
        double a, b, c0, d, e, f;
        while (true) {
            cout << "Affine matrix coefficients (six separate prompts):\n";
            a = askDouble("a: ");
            b = askDouble("b: ");
            c0 = askDouble("c: ");
            d = askDouble("d: ");
            e = askDouble("e: ");
            f = askDouble("f: ");
            if (abs(a*d-b*c0) >= 1e-12) break;
            cout<<"Error: matrix must be nonsingular (a*d - b*c != 0).\n";
        }
        string filename = askFilename("Output filename (.svg): ");
        SVGDocumentBuilder doc = builderFor(c);
        doc.addGradient(grad);
        auto group = make_unique<Group>();
        group->setId("bezier_composition");
        group->transform().matrix(a, b, c0, d, e, f);
        const double cx = c.width*0.5, cy = c.height*0.5;
        const double r = min(c.width, c.height)*0.28;
        const string paint = "url(#"+id+")";
        for (int i = 0; i < 4; ++i) {
            double s = 1.0-0.19*i;
            double R = r*s;
            double offset = (i-1.5)*r*0.11;
            auto p = make_unique<Path>();
            p->moveTo(cx-R+offset, cy) .cubicTo(cx-R+offset, cy-R*0.90, cx+R*0.30+offset, cy-R*1.35, cx+R+offset, cy) .quadTo(cx+R*0.55+offset, cy+R*1.18, cx+offset, cy+R*0.76) .cubicTo(cx-R*0.38+offset, cy+R*1.04, cx-R*0.85+offset, cy+R*0.45, cx-R+offset, cy) .closePath();
            p->setId("layer_"+to_string(i+1));
            p->setFill(paint).setStroke(i%2 == 0?"#14213D":"#FFFFFF", 2.2-i*0.3);
            p->setOpacity(1.0-0.12*i);
            p->transform().rotate(i*16, cx, cy);
            group->add(move(p));
        }
        doc.add(move(group));
        doc.build().save(filename);
        cout << "Saved: " << filename << " (4 closed Bezier paths)\n";
    }
}

int main() {
    cout << "=== SVG Vector Graphics Generator (C++17) ===\n";
    while (true) {
        cout << "\n1. Transformable rectangle (Rect)\n" << "2. Nested groups (Composite)\n" << "3. Bezier contour (Path)\n" << "4. Variant 1: Multi-stop linear gradient and Bezier shapes\n" << "0. Exit\n";
        try {
            int choice = askInt("Select menu item: ", 0, 4);
            if (choice == 0) {
                break;
            }
            try {
                switch (choice) {
                    case 1:
                        taskRect();
                        break;
                    case 2:
                        taskGroup();
                        break;
                    case 3:
                        taskPath();
                        break;
                    case 4:
                        taskVariant1();
                        break;
                }
            }
            catch (const exception& e) {
                cout << "Operation error: " << e.what() << "\n";
            }
        }
        catch (const exception& e) {
            cout << "Input finished: " << e.what() << "\n";
            break;
        }
    }
    return 0;
}
