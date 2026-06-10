#include "phantompad/client/trace.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "json_lite.hpp"  // reused from pc/client/proto/tests (header-only)

namespace phantompad::client {

namespace {

// Accept either a JSON bool or an int (0/1) for tip/confidence fields.
bool readFlag(const json::Value& v, bool dflt) {
    switch (v.type()) {
        case json::Type::Bool:
            return v.as_bool();
        case json::Type::Int:
            return v.as_int() != 0;
        default:
            return dflt;
    }
}

proto::TouchFrame parseFrame(const json::Value& obj) {
    proto::TouchFrame frame;
    frame.timestampUs = static_cast<std::uint32_t>(obj.at("t").as_int());
    if (obj.contains("contacts")) {
        for (const auto& cv : obj.at("contacts").as_array()) {
            proto::Contact c;
            c.id = static_cast<std::uint8_t>(cv.at("id").as_int());
            c.tip = readFlag(cv.at("tip"), false);
            c.confidence = cv.contains("confidence") ? readFlag(cv.at("confidence"), true) : true;
            c.x = static_cast<std::uint16_t>(cv.at("x").as_int());
            c.y = static_cast<std::uint16_t>(cv.at("y").as_int());
            frame.contacts.push_back(c);
        }
    }
    return frame;
}

}  // namespace

std::vector<TimedFrame> readTrace(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("trace: cannot open '" + path + "'");

    std::vector<TimedFrame> out;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        // Skip blank lines and '#' comments (handy for hand-authored fixtures).
        std::size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') continue;
        try {
            const json::Value obj = json::parse(line);
            TimedFrame tf;
            tf.frame = parseFrame(obj);
            tf.t = tf.frame.timestampUs;
            out.push_back(std::move(tf));
        } catch (const std::exception& e) {
            throw std::runtime_error("trace: " + path + ":" + std::to_string(lineNo) +
                                     ": " + e.what());
        }
    }
    return out;
}

std::string toTraceLine(std::uint32_t t, const proto::TouchFrame& frame) {
    std::ostringstream os;
    os << "{\"t\": " << t << ", \"contacts\": [";
    for (std::size_t i = 0; i < frame.contacts.size(); ++i) {
        const proto::Contact& c = frame.contacts[i];
        if (i != 0) os << ", ";
        os << "{\"id\": " << static_cast<int>(c.id) << ", \"tip\": " << (c.tip ? 1 : 0)
           << ", \"x\": " << c.x << ", \"y\": " << c.y;
        if (!c.confidence) os << ", \"confidence\": 0";
        os << "}";
    }
    os << "]}";
    return os.str();
}

}  // namespace phantompad::client
