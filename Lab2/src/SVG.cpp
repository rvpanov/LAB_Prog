#include "SVG.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cctype>

namespace svg {
    namespace {
        string spaces(size_t n) {
            return string(n, ' ');
        }
        void point(Point p) {
            checkFinite(p.x, "x");
            checkFinite(p.y, "y");
        }
        string stopXml(const Stop& s, size_t indent) {
            return spaces(indent) + "<stop offset=\"" + number(s.offset) + "\" stop-color=\"" + escape(s.color)
                + "\" stop-opacity=\"" + number(s.opacity) + "\"/>\n";
        }
        void validateStop(const Stop& s) {
            checkFinite(s.offset, "offset");
            checkFinite(s.opacity, "stop-opacity");
            if (s.offset < 0 || s.offset > 1 || s.opacity < 0 || s.opacity > 1) {
                throw invalid_argument("Offset and stop opacity must be in [0, 1]");
            }
            checkColor(s.color);
        }
        string pointsText(const vector<Point>& p) {
            string result;
            for (auto v : p) {
                if (!result.empty()) result += " ";
                result += number(v.x) + "," + number(v.y);
            }
            return result;
        }
    }

    string number(double value) {
        checkFinite(value, "number");
        ostringstream stream;
        stream << fixed << setprecision(6) << value;
        string s = stream.str();
        if (s.find('.') != string::npos) {
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
        }
        return (s == "-0") ? "0" : s;
    }

    string escape(const string& value) {
        string out;
        for (char c : value) {
            switch (c) {
                case '&':
                    out += "&amp;";
                    break;
                case '<':
                    out += "&lt;";
                    break;
                case '>':
                    out += "&gt;";
                    break;
                case '"':
                    out += "&quot;";
                    break;
                case '\'':
                    out += "&apos;";
                    break;
                default:
                    out += c;
            }
        }
        return out;
    }

    void checkFinite(double v, const string& name) {
        if (!isfinite(v) || abs(v) > 1e7) throw invalid_argument("Invalid " + name);
    }

    void checkPositive(double v, const string& name) {
        checkFinite(v, name);
        if (v <= 0) throw invalid_argument(name + " must be positive");
    }

    void checkNonnegative(double v, const string& name) {
        checkFinite(v, name);
        if (v < 0) throw invalid_argument(name + " must be nonnegative");
    }

    void checkId(const string& value) {
        if (value.empty() || !(isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) {
            throw invalid_argument("Invalid SVG id");
        }
        for (char c : value) {
            if (!(isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.')) throw invalid_argument("Invalid SVG id");
        }
    }

    void checkColor(const string& value) {
        if (value.size() == 4 || value.size() == 7) {
            if (value[0] == '#' && all_of(value.begin() + 1, value.end(), [](unsigned char c) { return isxdigit(c) != 0; })) return;
        }
        const vector<string> named = {
            "none", "black", "white", "red", "green", "blue", "yellow", "orange",
            "purple", "pink", "gray", "grey", "navy", "teal", "cyan", "magenta",
            "lime", "silver", "maroon", "olive", "aqua", "fuchsia", "transparent",
            "gold", "brown", "coral", "indigo", "violet", "lightblue", "darkblue"
        };
        if (find(named.begin(), named.end(), value) != named.end()) return;
        if (value.size() >= 7 && value.substr(0, 4) == "rgb(" && value.back() == ')') {
            string body = value.substr(4, value.size() - 5);
            istringstream in(body);
            int r, g, b;
            char c1, c2;
            string extra;
            if ((in >> r >> c1 >> g >> c2 >> b) && c1 == ',' && c2 == ',' && !(in >> extra) && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                return;
            }
        }
        if (value.size() > 6 && value.substr(0, 5) == "url(#" && value.back() == ')') {
            checkId(value.substr(5, value.size() - 6));
            return;
        }
        throw invalid_argument("Invalid color: use #RGB, #RRGGBB, name or rgb(r,g,b)");
    }

    Transform& Transform::translate(double x, double y) {
        checkFinite(x, "dx");
        checkFinite(y, "dy");
        commands_ += (commands_.empty() ? "" : " ") + string("translate(") + number(x) + " " + number(y) + ")";
        return *this;
    }

    Transform& Transform::rotate(double a, double x, double y) {
        checkFinite(a, "angle");
        checkFinite(x, "cx");
        checkFinite(y, "cy");
        commands_ += (commands_.empty() ? "" : " ") + string("rotate(") + number(a) + " " + number(x) + " " + number(y) + ")";
        return *this;
    }

    Transform& Transform::scale(double x, double y) {
        checkFinite(x, "sx");
        checkFinite(y, "sy");
        if (x == 0 || y == 0) throw invalid_argument("Scale cannot be zero");
        commands_ += (commands_.empty() ? "" : " ") + string("scale(") + number(x) + " " + number(y) + ")";
        return *this;
    }

    Transform& Transform::matrix(double a, double b, double c, double d, double e, double f) {
        for (double v : { a, b, c, d, e, f }) checkFinite(v, "matrix coefficient");
        if (abs(a * d - b * c) < 1e-12) {
            throw invalid_argument("Transformation matrix is singular");
        }
        commands_ += (commands_.empty() ? "" : " ") + string("matrix(") + number(a) + " " + number(b)
            + " " + number(c) + " " + number(d) + " " + number(e) + " " + number(f) + ")";
        return *this;
    }

    SVGElement& SVGElement::setId(const string& id) {
        checkId(id);
        id_ = id;
        return *this;
    }

    SVGElement& SVGElement::setClass(const string& name) {
        checkId(name);
        cssClass_ = name;
        return *this;
    }

    SVGElement& SVGElement::setFill(const string& c) {
        checkColor(c);
        fill_ = c;
        return *this;
    }

    SVGElement& SVGElement::setStroke(const string& c, double width) {
        checkColor(c);
        checkNonnegative(width, "stroke width");
        stroke_ = c;
        strokeWidth_ = width;
        return *this;
    }

    SVGElement& SVGElement::setOpacity(double v) {
        checkFinite(v, "opacity");
        if (v < 0 || v > 1) throw invalid_argument("Opacity must be [0,1]");
        opacity_ = v;
        return *this;
    }

    string SVGElement::attributes() const {
        string s;
        if (!id_.empty()) s += " id=\"" + escape(id_) + "\"";
        if (!cssClass_.empty()) s += " class=\"" + escape(cssClass_) + "\"";
        s += " fill=\"" + escape(fill_) + "\" stroke=\"" + escape(stroke_) + "\" stroke-width=\""
            + number(strokeWidth_) + "\" opacity=\"" + number(opacity_) + "\"";
        if (!transform_.str().empty()) s += " transform=\"" + escape(transform_.str()) + "\"";
        return s;
    }

    Rect::Rect(double x, double y, double w, double h, double rx, double ry) : x_(x), y_(y), width_(w), height_(h), rx_(rx), ry_(ry) {
        checkFinite(x, "x");
        checkFinite(y, "y");
        checkPositive(w, "width");
        checkPositive(h, "height");
        checkNonnegative(rx, "rx");
        checkNonnegative(ry, "ry");
        if (rx > w / 2 || ry > h / 2) {
            throw invalid_argument("Corner radii cannot exceed half the dimensions");
        }
    }

    string Rect::render(size_t n) const {
        return spaces(n) + "<rect x=\"" + number(x_) + "\" y=\"" + number(y_) + "\" width=\"" + number(width_)
            + "\" height=\"" + number(height_) + "\" rx=\"" + number(rx_) + "\" ry=\"" + number(ry_) + "\""
            + attributes() + "/>\n";
    }

    unique_ptr<SVGElement> Rect::clone() const {
        return make_unique<Rect>(*this);
    }

    Circle::Circle(double cx, double cy, double r) : cx_(cx), cy_(cy), radius_(r) {
        checkFinite(cx, "cx");
        checkFinite(cy, "cy");
        checkPositive(r, "radius");
    }

    string Circle::render(size_t n) const {
        return spaces(n) + "<circle cx=\"" + number(cx_) + "\" cy=\"" + number(cy_) + "\" r=\""
            + number(radius_) + "\"" + attributes() + "/>\n";
    }

    unique_ptr<SVGElement> Circle::clone() const {
        return make_unique<Circle>(*this);
    }

    Line::Line(Point a, Point b) : a_(a), b_(b) {
        point(a);
        point(b);
    }

    string Line::render(size_t n) const {
        return spaces(n) + "<line x1=\"" + number(a_.x) + "\" y1=\"" + number(a_.y) + "\" x2=\""
            + number(b_.x) + "\" y2=\"" + number(b_.y) + "\"" + attributes() + "/>\n";
    }

    unique_ptr<SVGElement> Line::clone() const {
        return make_unique<Line>(*this);
    }

    Polyline::Polyline(vector<Point> p) : points_(move(p)) {
        if (points_.size() < 2) throw invalid_argument("Polyline needs at least two points");
        for (auto v : points_) point(v);
    }

    string Polyline::render(size_t n) const {
        return spaces(n) + "<polyline points=\"" + pointsText(points_) + "\"" + attributes() + "/>\n";
    }

    unique_ptr<SVGElement> Polyline::clone() const {
        return make_unique<Polyline>(*this);
    }

    Path& Path::moveTo(double x, double y) {
        point({ x, y });
        data_ += "M " + number(x) + " " + number(y) + " ";
        hasStart_ = true;
        return *this;
    }

    Path& Path::lineTo(double x, double y) {
        if (!hasStart_) throw logic_error("Call moveTo first");
        point({ x, y });
        data_ += "L " + number(x) + " " + number(y) + " ";
        return *this;
    }

    Path& Path::cubicTo(double x1, double y1, double x2, double y2, double x, double y) {
        if (!hasStart_) throw logic_error("Call moveTo first");
        point({ x1, y1 });
        point({ x2, y2 });
        point({ x, y });
        data_ += "C " + number(x1) + " " + number(y1) + " " + number(x2) + " " + number(y2) + " "
            + number(x) + " " + number(y) + " ";
        return *this;
    }

    Path& Path::quadTo(double x1, double y1, double x, double y) {
        if (!hasStart_) throw logic_error("Call moveTo first");
        point({ x1, y1 });
        point({ x, y });
        data_ += "Q " + number(x1) + " " + number(y1) + " " + number(x) + " " + number(y) + " ";
        return *this;
    }

    Path& Path::closePath() {
        if (!hasStart_) throw logic_error("Call moveTo first");
        data_ += "Z ";
        return *this;
    }

    Path& Path::setDasharray(const string& s) {
        if (s.empty()) {
            dasharray_.clear();
            return *this;
        }
        istringstream in(s);
        double value;
        if (!(in >> value)) throw invalid_argument("Invalid dasharray");
        checkNonnegative(value, "dash length");
        while (true) {
            in >> ws;
            if (in.eof()) break;
            char separator;
            if (!(in >> separator) || separator != ',' || !(in >> value)) {
                throw invalid_argument("Dasharray format: 4,2");
            }
            checkNonnegative(value, "dash length");
        }
        dasharray_ = s;
        return *this;
    }

    Path& Path::setLinecap(const string& c) {
        if (c != "butt" && c != "round" && c != "square") {
            throw invalid_argument("Invalid linecap");
        }
        linecap_ = c;
        return *this;
    }

    Path& Path::setLinejoin(const string& c) {
        if (c != "miter" && c != "round" && c != "bevel") {
            throw invalid_argument("Invalid linejoin");
        }
        linejoin_ = c;
        return *this;
    }

    string Path::render(size_t n) const {
        if (data_.empty()) throw logic_error("Empty path");
        string s = spaces(n) + "<path d=\"" + data_ + "\"" + attributes();
        if (!dasharray_.empty()) s += " stroke-dasharray=\"" + dasharray_ + "\"";
        s += " stroke-linecap=\"" + linecap_ + "\" stroke-linejoin=\"" + linejoin_ + "\"/>\n";
        return s;
    }

    unique_ptr<SVGElement> Path::clone() const {
        return make_unique<Path>(*this);
    }

    Group::Group(const Group& o) : SVGElement(o) {
        for (const auto& el : o.children_) children_.push_back(el->clone());
    }

    Group& Group::operator =(const Group& o) {
        if (this !=& o) {
            Group temp(o);
            *this = move(temp);
        }
        return *this;
    }

    void Group::add(unique_ptr<SVGElement> p) {
        if (!p) throw invalid_argument("Null child");
        children_.push_back(move(p));
    }

    string Group::render(size_t n) const {
        string s = spaces(n) + "<g" + attributes() + ">\n";
        for (const auto& el : children_) s += el->render(n + 2);
        s += spaces(n) + "</g>\n";
        return s;
    }

    unique_ptr<SVGElement> Group::clone() const {
        return make_unique<Group>(*this);
    }

    LinearGradient::LinearGradient(string id, double x1, double y1, double x2, double y2) : id_(move(id)), x1_(x1), y1_(y1), x2_(x2), y2_(y2) {
        checkId(id_);
        for (double v : { x1, y1, x2, y2 }) checkFinite(v, "gradient coordinate");
        if (x1 == x2 && y1 == y2) throw invalid_argument("Gradient axis must have length");
    }

    void LinearGradient::addStop(double offset, const string& c, double opacity) {
        Stop s{ offset, c, opacity };
        validateStop(s);
        if (!stops_.empty() && offset < stops_.back().offset) {
            throw invalid_argument("Offsets must be sorted");
        }
        stops_.push_back(s);
    }

    string LinearGradient::render(size_t n) const {
        if (stops_.size() < 2) throw logic_error("Gradient needs 2+ stops");
        string s = spaces(n) + "<linearGradient id=\"" + escape(id_) + "\" x1=\"" + number(x1_)
            + "%\" y1=\"" + number(y1_) + "%\" x2=\"" + number(x2_) + "%\" y2=\"" + number(y2_) + "%\">\n";
        for (auto& v : stops_) s += stopXml(v, n + 2);
        return s + spaces(n) + "</linearGradient>\n";
    }

    RadialGradient::RadialGradient(string id, double cx, double cy, double r, double fx, double fy) : id_(move(id)), cx_(cx), cy_(cy), r_(r), fx_(fx), fy_(fy) {
        checkId(id_);
        for (double v : { cx, cy, fx, fy }) checkFinite(v, "radial gradient coordinate");
        checkPositive(r, "radius");
    }

    void RadialGradient::addStop(double offset, const string& c, double opacity) {
        Stop s{ offset, c, opacity };
        validateStop(s);
        if (!stops_.empty() && offset < stops_.back().offset) {
            throw invalid_argument("Offsets must be sorted");
        }
        stops_.push_back(s);
    }

    string RadialGradient::render(size_t n) const {
        if (stops_.size() < 2) throw logic_error("Gradient needs 2+ stops");
        string s = spaces(n) + "<radialGradient id=\"" + escape(id_) + "\" cx=\"" + number(cx_)
            + "%\" cy=\"" + number(cy_) + "%\" r=\"" + number(r_) + "%\" fx=\"" + number(fx_) + "%\" fy=\""
            + number(fy_) + "%\">\n";
        for (auto& v : stops_) s += stopXml(v, n + 2);
        return s + spaces(n) + "</radialGradient>\n";
    }

    SVGDocument::SVGDocument(double w, double h, double x, double y, double vw, double vh, vector<string> defs, vector<unique_ptr<SVGElement>> el) : width_(w), height_(h), minX_(x), minY_(y), viewWidth_(vw), viewHeight_(vh), defs_(move(defs)), elements_(move(el)) {
    }

    string SVGDocument::render() const {
        string s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
            + number(width_) + "\" height=\"" + number(height_) + "\" viewBox=\"" + number(minX_) + " " + number(minY_)
            + " " + number(viewWidth_) + " " + number(viewHeight_) + "\">\n  <defs>\n";
        for (const auto& def : defs_) s += def;
        s += "  </defs>\n";
        for (const auto& el : elements_) s += el->render(2);
        return s + "</svg>\n";
    }

    void SVGDocument::save(const string& filename) const {
        ofstream out(filename, ios::binary);
        if (!out) throw runtime_error("Cannot open output file: " + filename);
        out << render();
        if (!out) throw runtime_error("Failed to write output file");
    }

    SVGDocumentBuilder& SVGDocumentBuilder::canvas(double w, double h) {
        checkPositive(w, "canvas width");
        checkPositive(h, "canvas height");
        width_ = w;
        height_ = h;
        return *this;
    }

    SVGDocumentBuilder& SVGDocumentBuilder::viewBox(double x, double y, double w, double h) {
        checkFinite(x, "viewBox x");
        checkFinite(y, "viewBox y");
        checkPositive(w, "viewBox width");
        checkPositive(h, "viewBox height");
        x_ = x;
        y_ = y;
        vw_ = w;
        vh_ = h;
        return *this;
    }

    SVGDocumentBuilder& SVGDocumentBuilder::addGradient(const LinearGradient& grad) {
        defs_.push_back(grad.render(4));
        return *this;
    }

    SVGDocumentBuilder& SVGDocumentBuilder::addGradient(const RadialGradient& grad) {
        defs_.push_back(grad.render(4));
        return *this;
    }

    SVGDocumentBuilder& SVGDocumentBuilder::add(unique_ptr<SVGElement> p) {
        if (!p) throw invalid_argument("Null element");
        elements_.push_back(move(p));
        return *this;
    }

    SVGDocument SVGDocumentBuilder::build() {
        return SVGDocument(width_, height_, x_, y_, vw_, vh_, move(defs_), move(elements_));
    }
}
