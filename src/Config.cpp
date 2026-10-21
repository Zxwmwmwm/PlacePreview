#include "placepreview/Config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace placepreview {

namespace {

std::string trimCopy(std::string_view sv) {
    std::size_t begin = 0;
    while (begin < sv.size() && std::isspace(static_cast<unsigned char>(sv[begin])) != 0) {
        ++begin;
    }
    std::size_t end = sv.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(sv[end - 1])) != 0) {
        --end;
    }
    return std::string(sv.substr(begin, end - begin));
}

bool parseBoolValue(std::string_view raw, bool fallback) noexcept {
    const std::string t = trimCopy(raw);
    if (t == "true" || t == "1" || t == "on" || t == "yes") return true;
    if (t == "false" || t == "0" || t == "off" || t == "no") return false;
    return fallback;
}

int parseIntValue(std::string_view raw, int fallback) noexcept {
    const std::string t = trimCopy(raw);
    if (t.empty()) return fallback;
    char* endPtr = nullptr;
    const long v = std::strtol(t.c_str(), &endPtr, 10);
    if (endPtr == t.c_str()) return fallback;
    return static_cast<int>(v);
}

float parseFloatValue(std::string_view raw, float fallback) noexcept {
    const std::string t = trimCopy(raw);
    if (t.empty()) return fallback;
    char* endPtr = nullptr;
    const float v = std::strtof(t.c_str(), &endPtr);
    if (endPtr == t.c_str() || !std::isfinite(v)) return fallback;
    return v;
}

std::string escapeJson(std::string_view sv) {
    std::string out;
    out.reserve(sv.size() + 4);
    for (char c : sv) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

bool extractJsonFieldRaw(std::string_view json, std::string_view key, std::string& outVal) {
    const std::string needle = "\"" + std::string(key) + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    pos += needle.size();
    while (pos < json.size() && (std::isspace(static_cast<unsigned char>(json[pos])) != 0 || json[pos] == ':')) {
        ++pos;
    }
    if (pos >= json.size()) return false;

    if (json[pos] == '"') {
        ++pos;
        std::string unescaped;
        while (pos < json.size()) {
            const char c = json[pos++];
            if (c == '\\' && pos < json.size()) {
                unescaped.push_back(json[pos++]);
            } else if (c == '"') {
                outVal = std::move(unescaped);
                return true;
            } else {
                unescaped.push_back(c);
            }
        }
        return false;
    }

    const std::size_t start = pos;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != '\n' && json[pos] != '\r') {
        ++pos;
    }
    outVal = trimCopy(json.substr(start, pos - start));
    return !outVal.empty();
}

void applyPresetInternal(ModConfig& cfg, std::string_view preset) {
    if (preset == "Frosted White") {
        cfg.preset = "Frosted White";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Custom Accent";
        cfg.fillOpacity = 0.34f;
        cfg.wireframeOpacity = 0.68f;
        cfg.accentRed = 255;
        cfg.accentGreen = 255;
        cfg.accentBlue = 255;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
        cfg.showOrientationGuides = false;
        cfg.subtleBreathingPulse = false;
    } else if (preset == "Classic Cyan") {
        cfg.preset = "Classic Cyan";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Custom Accent";
        cfg.fillOpacity = 0.28f;
        cfg.wireframeOpacity = 0.86f;
        cfg.accentRed = 92;
        cfg.accentGreen = 224;
        cfg.accentBlue = 255;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
        cfg.showOrientationGuides = true;
        cfg.subtleBreathingPulse = false;
    } else if (preset == "Material Adaptive" || preset == "Material Tint") {
        cfg.preset = "Material Adaptive";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Material Adaptive";
        cfg.fillOpacity = 0.34f;
        cfg.wireframeOpacity = 0.86f;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
        cfg.showOrientationGuides = false;
        cfg.subtleBreathingPulse = false;
    } else if (preset == "Crisp Wireframe") {
        cfg.preset = "Crisp Wireframe";
        cfg.renderMode = "Wireframe Only";
        cfg.colorMode = "Custom Accent";
        cfg.fillOpacity = 0.12f;
        cfg.wireframeOpacity = 0.94f;
        cfg.accentRed = 255;
        cfg.accentGreen = 255;
        cfg.accentBlue = 255;
        cfg.accentAlpha = 255;
        cfg.showOrientationGuides = false;
        cfg.subtleBreathingPulse = false;
    } else if (preset == "Subtle Minimal") {
        cfg.preset = "Subtle Minimal";
        cfg.renderMode = "Translucent Fill";
        cfg.colorMode = "Custom Accent";
        cfg.fillOpacity = 0.26f;
        cfg.wireframeOpacity = 0.48f;
        cfg.accentRed = 255;
        cfg.accentGreen = 255;
        cfg.accentBlue = 255;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
        cfg.showOrientationGuides = false;
        cfg.subtleBreathingPulse = false;
    } else if (preset == "High Contrast") {
        cfg.preset = "High Contrast";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Custom Accent";
        cfg.fillOpacity = 0.40f;
        cfg.wireframeOpacity = 1.00f;
        cfg.accentRed = 80;
        cfg.accentGreen = 255;
        cfg.accentBlue = 176;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
        cfg.showOrientationGuides = true;
        cfg.subtleBreathingPulse = true;
    } else {
        cfg.preset = "Custom";
    }
}

} // namespace

ConfigManager& ConfigManager::instance() noexcept {
    static ConfigManager mgr;
    return mgr;
}

bool ConfigManager::parseHexColor(
    std::string_view raw,
    int& outR,
    int& outG,
    int& outB,
    int& outA) noexcept {

    std::string s = trimCopy(raw);
    if (!s.empty() && s.front() == '#') {
        s.erase(0, 1);
    } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.erase(0, 2);
    }

    if (s.size() != 6 && s.size() != 8) {
        return false;
    }
    for (char c : s) {
        if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }

    char* endPtr = nullptr;
    const unsigned long val = std::strtoul(s.c_str(), &endPtr, 16);
    if (endPtr != s.c_str() + s.size()) {
        return false;
    }

    if (s.size() == 8) {
        // #AARRGGBB native LeviLauncher ModMenu Color Picker format:
        outA = static_cast<int>((val >> 24u) & 0xFFul);
        outR = static_cast<int>((val >> 16u) & 0xFFul);
        outG = static_cast<int>((val >> 8u) & 0xFFul);
        outB = static_cast<int>(val & 0xFFul);
    } else {
        // #RRGGBB 6-digit hex shorthand:
        outA = 255;
        outR = static_cast<int>((val >> 16u) & 0xFFul);
        outG = static_cast<int>((val >> 8u) & 0xFFul);
        outB = static_cast<int>(val & 0xFFul);
    }
    return true;
}

void ConfigManager::clampConfig(ModConfig& cfg) noexcept {
    cfg.schemaVersion = 3;
    cfg.fillOpacity = std::clamp(cfg.fillOpacity, 0.05f, 0.85f);
    cfg.wireframeOpacity = std::clamp(cfg.wireframeOpacity, 0.10f, 1.00f);
    cfg.accentRed = std::clamp(cfg.accentRed, 0, 255);
    cfg.accentGreen = std::clamp(cfg.accentGreen, 0, 255);
    cfg.accentBlue = std::clamp(cfg.accentBlue, 0, 255);
    cfg.accentAlpha = std::clamp(cfg.accentAlpha, 25, 255);
    cfg.animationSpeed = std::clamp(cfg.animationSpeed, 4.0f, 36.0f);

    if (cfg.renderMode != "Shaded + Wireframe" &&
        cfg.renderMode != "Translucent Fill" &&
        cfg.renderMode != "Wireframe Only") {
        cfg.renderMode = "Shaded + Wireframe";
    }

    if (cfg.colorMode != "Material Adaptive" && cfg.colorMode != "Custom Accent") {
        cfg.colorMode = "Custom Accent";
    }

    if (cfg.preset == "Material Tint") {
        cfg.preset = "Material Adaptive";
    }
    if (cfg.preset != "Frosted White" &&
        cfg.preset != "Classic Cyan" &&
        cfg.preset != "Material Adaptive" &&
        cfg.preset != "Crisp Wireframe" &&
        cfg.preset != "Subtle Minimal" &&
        cfg.preset != "High Contrast" &&
        cfg.preset != "Custom") {
        cfg.preset = "Frosted White";
    }
}

void ConfigManager::init(const std::filesystem::path& configDir) {
    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);
    const std::filesystem::path target = configDir / "placepreview.json";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        configFilePath_ = target;
    }
    if (!loadFromFile(target)) {
        (void)saveToFile(target);
    }
}

ModConfig ConfigManager::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void ConfigManager::setEnabled(bool enabled) {
    std::filesystem::path savePath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.enabled = enabled;
        savePath = configFilePath_;
    }
    if (!savePath.empty()) {
        (void)saveToFile(savePath);
    }
}

void ConfigManager::applyPreset(std::string_view presetName) {
    std::filesystem::path savePath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        applyPresetInternal(config_, presetName);
        clampConfig(config_);
        savePath = configFilePath_;
    }
    if (!savePath.empty()) {
        (void)saveToFile(savePath);
    }
}

void ConfigManager::applyKeyValue(std::string_view key, std::string_view value) {
    std::filesystem::path savePath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (key == "preset") {
            applyPresetInternal(config_, value);
        } else if (key == "previewColor") {
            int r = config_.accentRed;
            int g = config_.accentGreen;
            int b = config_.accentBlue;
            int a = config_.accentAlpha;
            if (parseHexColor(value, r, g, b, a)) {
                config_.accentRed = r;
                config_.accentGreen = g;
                config_.accentBlue = b;
                config_.accentAlpha = a;
                config_.colorMode = "Custom Accent";
                config_.preset = "Custom";
            }
        } else if (key == "enabled") {
            config_.enabled = parseBoolValue(value, config_.enabled);
        } else if (key == "renderMode") {
            config_.renderMode = trimCopy(value);
            config_.preset = "Custom";
        } else if (key == "colorMode") {
            config_.colorMode = trimCopy(value);
            config_.preset = "Custom";
        } else if (key == "fillOpacity") {
            config_.fillOpacity = parseFloatValue(value, config_.fillOpacity);
            config_.preset = "Custom";
        } else if (key == "wireframeOpacity") {
            config_.wireframeOpacity = parseFloatValue(value, config_.wireframeOpacity);
            config_.preset = "Custom";
        } else if (key == "accentRed") {
            config_.accentRed = parseIntValue(value, config_.accentRed);
            config_.colorMode = "Custom Accent";
            config_.preset = "Custom";
        } else if (key == "accentGreen") {
            config_.accentGreen = parseIntValue(value, config_.accentGreen);
            config_.colorMode = "Custom Accent";
            config_.preset = "Custom";
        } else if (key == "accentBlue") {
            config_.accentBlue = parseIntValue(value, config_.accentBlue);
            config_.colorMode = "Custom Accent";
            config_.preset = "Custom";
        } else if (key == "smoothAnimation") {
            config_.smoothAnimation = parseBoolValue(value, config_.smoothAnimation);
        } else if (key == "animationSpeed") {
            config_.animationSpeed = parseFloatValue(value, config_.animationSpeed);
        } else if (key == "directionalShading") {
            config_.directionalShading = parseBoolValue(value, config_.directionalShading);
        } else if (key == "showOrientationGuides") {
            config_.showOrientationGuides = parseBoolValue(value, config_.showOrientationGuides);
        } else if (key == "subtleBreathingPulse") {
            config_.subtleBreathingPulse = parseBoolValue(value, config_.subtleBreathingPulse);
        } else if (key == "hideOnInteractiveWithoutSneak") {
            config_.hideOnInteractiveWithoutSneak = parseBoolValue(value, config_.hideOnInteractiveWithoutSneak);
        }
        clampConfig(config_);
        savePath = configFilePath_;
    }
    if (!savePath.empty()) {
        (void)saveToFile(savePath);
    }
}

void ConfigManager::resetDefaults() {
    std::filesystem::path savePath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = ModConfig{};
        clampConfig(config_);
        savePath = configFilePath_;
    }
    if (!savePath.empty()) {
        (void)saveToFile(savePath);
    }
}

bool ConfigManager::loadFromFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string raw = ss.str();
    const ModConfig parsed = parseJson(raw);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = parsed;
    }
    return true;
}

bool ConfigManager::saveToFile(const std::filesystem::path& path) const {
    if (path.empty()) return false;
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    const std::string serialized = serializeJson(snapshot());
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;
    out << serialized;
    return out.good();
}

std::string ConfigManager::serializeJson(const ModConfig& cfg) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"schemaVersion\": " << cfg.schemaVersion << ",\n"
       << "  \"enabled\": " << (cfg.enabled ? "true" : "false") << ",\n"
       << "  \"preset\": \"" << escapeJson(cfg.preset) << "\",\n"
       << "  \"renderMode\": \"" << escapeJson(cfg.renderMode) << "\",\n"
       << "  \"colorMode\": \"" << escapeJson(cfg.colorMode) << "\",\n"
       << "  \"previewColor\": \"" << escapeJson(cfg.previewColorHex()) << "\",\n"
       << "  \"fillOpacity\": " << cfg.fillOpacity << ",\n"
       << "  \"wireframeOpacity\": " << cfg.wireframeOpacity << ",\n"
       << "  \"smoothAnimation\": " << (cfg.smoothAnimation ? "true" : "false") << ",\n"
       << "  \"animationSpeed\": " << cfg.animationSpeed << ",\n"
       << "  \"directionalShading\": " << (cfg.directionalShading ? "true" : "false") << ",\n"
       << "  \"showOrientationGuides\": " << (cfg.showOrientationGuides ? "true" : "false") << ",\n"
       << "  \"subtleBreathingPulse\": " << (cfg.subtleBreathingPulse ? "true" : "false") << ",\n"
       << "  \"hideOnInteractiveWithoutSneak\": " << (cfg.hideOnInteractiveWithoutSneak ? "true" : "false") << "\n"
       << "}\n";
    return ss.str();
}

ModConfig ConfigManager::parseJson(std::string_view jsonText) {
    ModConfig cfg{};
    std::string v;
    if (extractJsonFieldRaw(jsonText, "preset", v)) applyPresetInternal(cfg, v);
    if (extractJsonFieldRaw(jsonText, "enabled", v)) cfg.enabled = parseBoolValue(v, cfg.enabled);
    if (extractJsonFieldRaw(jsonText, "renderMode", v)) cfg.renderMode = v;
    if (extractJsonFieldRaw(jsonText, "colorMode", v)) cfg.colorMode = v;
    if (extractJsonFieldRaw(jsonText, "previewColor", v)) {
        int r = cfg.accentRed;
        int g = cfg.accentGreen;
        int b = cfg.accentBlue;
        int a = cfg.accentAlpha;
        if (parseHexColor(v, r, g, b, a)) {
            cfg.accentRed = r;
            cfg.accentGreen = g;
            cfg.accentBlue = b;
            cfg.accentAlpha = a;
        }
    }
    if (extractJsonFieldRaw(jsonText, "accentRed", v)) cfg.accentRed = parseIntValue(v, cfg.accentRed);
    if (extractJsonFieldRaw(jsonText, "accentGreen", v)) cfg.accentGreen = parseIntValue(v, cfg.accentGreen);
    if (extractJsonFieldRaw(jsonText, "accentBlue", v)) cfg.accentBlue = parseIntValue(v, cfg.accentBlue);
    if (extractJsonFieldRaw(jsonText, "fillOpacity", v)) cfg.fillOpacity = parseFloatValue(v, cfg.fillOpacity);
    if (extractJsonFieldRaw(jsonText, "wireframeOpacity", v)) cfg.wireframeOpacity = parseFloatValue(v, cfg.wireframeOpacity);
    if (extractJsonFieldRaw(jsonText, "smoothAnimation", v)) cfg.smoothAnimation = parseBoolValue(v, cfg.smoothAnimation);
    if (extractJsonFieldRaw(jsonText, "animationSpeed", v)) cfg.animationSpeed = parseFloatValue(v, cfg.animationSpeed);
    if (extractJsonFieldRaw(jsonText, "directionalShading", v)) cfg.directionalShading = parseBoolValue(v, cfg.directionalShading);
    if (extractJsonFieldRaw(jsonText, "showOrientationGuides", v)) cfg.showOrientationGuides = parseBoolValue(v, cfg.showOrientationGuides);
    if (extractJsonFieldRaw(jsonText, "subtleBreathingPulse", v)) cfg.subtleBreathingPulse = parseBoolValue(v, cfg.subtleBreathingPulse);
    if (extractJsonFieldRaw(jsonText, "hideOnInteractiveWithoutSneak", v)) {
        cfg.hideOnInteractiveWithoutSneak = parseBoolValue(v, cfg.hideOnInteractiveWithoutSneak);
    }
    clampConfig(cfg);
    return cfg;
}

} // namespace placepreview
