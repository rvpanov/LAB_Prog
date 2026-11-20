#pragma once
#include <memory>
#include <string>
#include <vector>
#include <utility>

using namespace std;

namespace svg {
string number(double value);
string escape(const string& value);
void checkFinite(double value, const string& name);
void checkPositive(double value, const string& name);
void checkNonnegative(double value, const string& name);
void checkColor(const string& value);
void checkId(const string& value);

struct Point { double x, y; };

class Transform {
    string commands_;
public:
    Transform& translate(double dx, double dy);
    Transform& rotate(double angle, double cx = 0, double cy = 0);
    Transform& scale(double sx, double sy);
    Transform& matrix(double a, double b, double c, double d, double e, double f);
    const string& str() const { return commands_; }
};

class SVGElement {
protected:
    string id_, cssClass_, fill_ = "none", stroke_ = "none";
    double strokeWidth_ = 1, opacity_ = 1;
    Transform transform_;
    string attributes() const;
public:
    virtual ~SVGElement() = default;
    SVGElement& setId(const string& id);
    SVGElement& setClass(const string& name);
    SVGElement& setFill(const string& color);
    SVGElement& setStroke(const string& color, double width = 1);
    SVGElement& setOpacity(double value);
    Transform& transform() { return transform_; }
    virtual string render(size_t indent = 0) const = 0;
    virtual unique_ptr<SVGElement> clone() const = 0;
};

class Rect : public SVGElement {
    double x_, y_, width_, height_, rx_, ry_;
public:
    Rect(double x, double y, double width, double height, double rx = 0, double ry = 0);
    string render(size_t indent = 0) const override;
    unique_ptr<SVGElement> clone() const override;
};
class Circle : public SVGElement {
    double cx_, cy_, radius_;
public:
    Circle(double cx, double cy, double radius);
    string render(size_t indent = 0) const override;
    unique_ptr<SVGElement> clone() const override;
};
class Line : public SVGElement {
    Point a_, b_;
public:
    Line(Point a, Point b);
    string render(size_t indent = 0) const override;
    unique_ptr<SVGElement> clone() const override;
};
class Polyline : public SVGElement {
    vector<Point> points_;
public:
    explicit Polyline(vector<Point> points);
    string render(size_t indent = 0) const override;
    unique_ptr<SVGElement> clone() const override;
};
class Path : public SVGElement {
    string data_, dasharray_ = "";
    string linecap_ = "butt", linejoin_ = "miter";
    bool hasStart_ = false;
public:
    Path& moveTo(double x, double y);
    Path& lineTo(double x, double y);
    Path& cubicTo(double x1, double y1, double x2, double y2, double x, double y);
    Path& quadTo(double x1, double y1, double x, double y);
    Path& closePath();
    Path& setDasharray(const string& dasharray);
    Path& setLinecap(const string& cap);
    Path& setLinejoin(const string& join);
    string render(size_t indent = 0) const override;
    unique_ptr<SVGElement> clone() const override;
};
class Group : public SVGElement {
    vector<unique_ptr<SVGElement>> children_;
public:
    Group() = default;
    Group(const Group& other);
    Group& operator=(const Group& other);
    Group(Group&&) = default;
    Group& operator=(Group&&) = default;
    void add(unique_ptr<SVGElement> child);
    size_t size() const { return children_.size(); }
    string render(size_t indent = 0) const override;
    unique_ptr<SVGElement> clone() const override;
};

struct Stop { double offset; string color; double opacity = 1; };
class LinearGradient {
    string id_;
    double x1_, y1_, x2_, y2_;
    vector<Stop> stops_;
public:
    LinearGradient(string id, double x1, double y1, double x2, double y2);
    void addStop(double offset, const string& color, double opacity = 1);
    const string& id() const { return id_; }
    string render(size_t indent = 0) const;
};
class RadialGradient {
    string id_;
    double cx_, cy_, r_, fx_, fy_;
    vector<Stop> stops_;
public:
    RadialGradient(string id, double cx, double cy, double r, double fx, double fy);
    void addStop(double offset, const string& color, double opacity = 1);
    string render(size_t indent = 0) const;
};
class SVGDocument {
    double width_, height_, minX_, minY_, viewWidth_, viewHeight_;
    vector<string> defs_;
    vector<unique_ptr<SVGElement>> elements_;
public:
    SVGDocument(double w, double h, double x, double y, double vw, double vh,
                vector<string> defs, vector<unique_ptr<SVGElement>> elements);
    string render() const;
    void save(const string& filename) const;
};
class SVGDocumentBuilder {
    double width_ = 800, height_ = 600;
    double x_ = 0, y_ = 0, vw_ = 800, vh_ = 600;
    vector<string> defs_;
    vector<unique_ptr<SVGElement>> elements_;
public:
    SVGDocumentBuilder& canvas(double width, double height);
    SVGDocumentBuilder& viewBox(double x, double y, double width, double height);
    SVGDocumentBuilder& addGradient(const LinearGradient& gradient);
    SVGDocumentBuilder& addGradient(const RadialGradient& gradient);
    SVGDocumentBuilder& add(unique_ptr<SVGElement> element);
    SVGDocument build();
};
}
