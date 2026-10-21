#include "placepreview/Config.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace placepreview {

namespace {

std::string jsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

bool extractRawField(std::string_view json, std::string_view key, std::string_view& outToken) {
    const std::string quotedKey = "\"" + std::string(key) + "\"";
    const std::size_t kpos = json.find(quotedKey);
    if (kpos == std::string_view::npos) return false;
    const std::size_t colon = json.find(':', kpos + quotedKey.size());
    if (colon == std::string_view::npos) return false;
    std::size_t pos = colon + 1;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos >= json.size()) return false;

    if (json[pos] == '"') {
        std::size_t end = pos + 1;
        while (end < json.size()) {
            if (json[end] == '\\') {
                end += 2;
                continue;
            }
            if (json[end] == '"') {
                outToken = json.substr(pos + 1, end - pos - 1);
                return true;
            }
            ++end;
        }
        return false;
    }

    std::size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']' &&
           !std::isspace(static_cast<unsigned char>(json[end]))) {
        ++end;
    }
    outToken = json.substr(pos, end - pos);
    return !outToken.empty();
}

bool parseBoolToken(std::string_view tok, bool fallback) {
    if (tok == "true" || tok == "1") return true;
    if (tok == "false" || tok == "0") return false;
    return fallback;
}

int parseIntToken(std::string_view tok, int fallback, int minV, int maxV) {
    int val = fallback;
    const auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val);
    (void)ptr;
    if (ec != std::errc{}) return fallback;
    return std::clamp(val, minV, maxV);
}

float parseFloatToken(std::string_view tok, float fallback, float minV, float maxV) {
    std::string tmp(tok);
    char* endPtr = nullptr;
    const float val = std::strtof(tmp.c_str(), &endPtr);
    if (endPtr == tmp.c_str() || !std::isfinite(val)) return fallback;
    return std::clamp(val, minV, maxV);
}

int hexNibble(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool writeTextFileRobust(const std::filesystem::path& path, const std::string& data) {
    if (path.empty()) return false;
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    const std::filesystem::path tmpPath = path.string() + ".tmp";
    {
        std::ofstream ofs(tmpPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (ofs.is_open()) {
            ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
            ofs.flush();
            if (ofs.good()) {
                ofs.close();
                ec.clear();
                std::filesystem::rename(tmpPath, path, ec);
                if (!ec) {
                    return true;
                }
                // Android FUSE / Scoped Storage fallback when rename over existing file fails:
                ec.clear();
                std::filesystem::copy_file(
                    tmpPath, path, std::filesystem::copy_options::overwrite_existing, ec);
                std::error_code rmEc;
                std::filesystem::remove(tmpPath, rmEc);
                if (!ec) {
                    return true;
                }
            }
        }
    }

    // Final direct write fallback if temporary file creation/copy fails on restricted storage:
    std::error_code rmEc;
    std::filesystem::remove(tmpPath, rmEc);
    std::ofstream directOfs(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!directOfs.is_open()) return false;
    directOfs.write(data.data(), static_cast<std::streamsize>(data.size()));
    directOfs.flush();
    return directOfs.good();
}

void applyPresetUnlocked(ModConfig& cfg, std::string_view presetName) {
    if (presetName == "Frosted White" || presetName == "0") {
        cfg.preset = "Frosted White";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Custom Color";
        cfg.fillOpacity = 0.34f;
        cfg.wireframeOpacity = 0.72f;
        cfg.accentRed = 255;
        cfg.accentGreen = 255;
        cfg.accentBlue = 255;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
    } else if (presetName == "Classic Cyan" || presetName == "1") {
        cfg.preset = "Classic Cyan";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Custom Color";
        cfg.fillOpacity = 0.34f;
        cfg.wireframeOpacity = 0.85f;
        cfg.accentRed = 72;
        cfg.accentGreen = 200;
        cfg.accentBlue = 255;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
    } else if (presetName == "Material Adaptive" || presetName == "2") {
        cfg.preset = "Material Adaptive";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Material Adaptive";
        cfg.fillOpacity = 0.38f;
        cfg.wireframeOpacity = 0.85f;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
    } else if (presetName == "Crisp Wireframe" || presetName == "3") {
        cfg.preset = "Crisp Wireframe";
        cfg.renderMode = "Wireframe Only";
        cfg.colorMode = "Custom Color";
        // Preserve non-zero fillOpacity so switching Render Style back to Shaded/Translucent Fill
        // is immediately visible without manual slider recovery.
        if (cfg.fillOpacity < 0.10f) {
            cfg.fillOpacity = 0.34f;
        }
        cfg.wireframeOpacity = 0.92f;
        cfg.accentRed = 255;
        cfg.accentGreen = 255;
        cfg.accentBlue = 255;
        cfg.accentAlpha = 255;
    } else if (presetName == "Subtle Minimal" || presetName == "4") {
        cfg.preset = "Subtle Minimal";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Custom Color";
        cfg.fillOpacity = 0.16f;
        cfg.wireframeOpacity = 0.42f;
        cfg.accentRed = 235;
        cfg.accentGreen = 245;
        cfg.accentBlue = 255;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
    } else if (presetName == "High Contrast" || presetName == "5") {
        cfg.preset = "High Contrast";
        cfg.renderMode = "Shaded + Wireframe";
        cfg.colorMode = "Custom Color";
        cfg.fillOpacity = 0.48f;
        cfg.wireframeOpacity = 1.00f;
        cfg.accentRed = 255;
        cfg.accentGreen = 212;
        cfg.accentBlue = 59;
        cfg.accentAlpha = 255;
        cfg.directionalShading = true;
    } else if (presetName == "Custom" || presetName == "6") {
        cfg.preset = "Custom";
    }
}

} // namespace

bool ConfigManager::parseHexColor(
    std::string_view hexStr,
    int& outR,
    int& outG,
    int& outB,
    int& outA) noexcept {

    while (!hexStr.empty() && std::isspace(static_cast<unsigned char>(hexStr.front()))) {
        hexStr.remove_prefix(1);
    }
    while (!hexStr.empty() && std::isspace(static_cast<unsigned char>(hexStr.back()))) {
        hexStr.remove_suffix(1);
    }
    if (!hexStr.empty() && hexStr.front() == '#') {
        hexStr.remove_prefix(1);
    }

    auto readByte = [&](std::size_t idx, int& dst) -> bool {
        const int hi = hexNibble(hexStr[idx]);
        const int lo = hexNibble(hexStr[idx + 1]);
        if (hi < 0 || lo < 0) return false;
        dst = (hi << 4) | lo;
        return true;
    };

    if (hexStr.size() == 8) {
        int a = 255, r = 255, g = 255, b = 255;
        if (!readByte(0, a) || !readByte(2, r) || !readByte(4, g) || !readByte(6, b)) {
            return false;
        }
        outA = a;
        outR = r;
        outG = g;
        outB = b;
        return true;
    }
    if (hexStr.size() == 6) {
        int r = 255, g = 255, b = 255;
        if (!readByte(0, r) || !readByte(2, g) || !readByte(4, b)) {
            return false;
        }
        outA = 255;
        outR = r;
        outG = g;
        outB = b;
        return true;
    }
    return false;
}

ConfigManager& ConfigManager::instance() noexcept {
    static ConfigManager sInst;
    return sInst;
}

void ConfigManager::init(const std::filesystem::path& configDir) {
    std::filesystem::path targetPath;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        std::error_code ec;
        std::filesystem::create_directories(configDir, ec);
        mFilePath = configDir / "placepreview.json";
        targetPath = mFilePath;
    }

    if (!loadFromFile(targetPath)) {
        (void)saveToFile(targetPath);
    }
}

ModConfig ConfigManager::snapshot() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mConfig;
}

void ConfigManager::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConfig.enabled = enabled;
    saveLocked();
}

void ConfigManager::applyPreset(std::string_view presetName) {
    std::lock_guard<std::mutex> lock(mMutex);
    applyPresetUnlocked(mConfig, presetName);
    saveLocked();
}

void ConfigManager::resetDefaults() {
    std::lock_guard<std::mutex> lock(mMutex);
    mConfig = ModConfig{};
    saveLocked();
}

void ConfigManager::applyKeyValue(std::string_view key, std::string_view value) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (key == "enabled") {
        mConfig.enabled = parseBoolToken(value, mConfig.enabled);
    } else if (key == "preset") {
        applyPresetUnlocked(mConfig, value);
    } else if (key == "previewColor") {
        int r = mConfig.accentRed;
        int g = mConfig.accentGreen;
        int b = mConfig.accentBlue;
        int a = mConfig.accentAlpha;
        if (parseHexColor(value, r, g, b, a)) {
            mConfig.accentRed = r;
            mConfig.accentGreen = g;
            mConfig.accentBlue = b;
            mConfig.accentAlpha = a;
            mConfig.colorMode = "Custom Color";
            mConfig.preset = "Custom";
        }
    } else if (key == "renderMode") {
        if (value == "Shaded + Wireframe" || value == "0") {
            mConfig.renderMode = "Shaded + Wireframe";
            if (mConfig.fillOpacity < 0.05f) mConfig.fillOpacity = 0.34f;
            if (mConfig.wireframeOpacity < 0.05f) mConfig.wireframeOpacity = 0.72f;
            mConfig.preset = "Custom";
        } else if (value == "Wireframe Only" || value == "1") {
            mConfig.renderMode = "Wireframe Only";
            if (mConfig.wireframeOpacity < 0.05f) mConfig.wireframeOpacity = 0.85f;
            mConfig.preset = "Custom";
        } else if (value == "Translucent Fill" || value == "2") {
            mConfig.renderMode = "Translucent Fill";
            if (mConfig.fillOpacity < 0.05f) mConfig.fillOpacity = 0.38f;
            mConfig.preset = "Custom";
        }
    } else if (key == "colorMode") {
        if (value == "Material Adaptive" || value == "1") {
            mConfig.colorMode = "Material Adaptive";
            mConfig.preset = "Custom";
        } else if (value == "Custom Color" || value == "Custom Accent" ||
                   value == "Preview Color" || value == "0") {
            mConfig.colorMode = "Custom Color";
            mConfig.preset = "Custom";
        }
    } else if (key == "fillOpacity") {
        mConfig.fillOpacity = parseFloatToken(value, mConfig.fillOpacity, 0.0f, 1.0f);
        mConfig.preset = "Custom";
    } else if (key == "wireframeOpacity") {
        mConfig.wireframeOpacity = parseFloatToken(value, mConfig.wireframeOpacity, 0.0f, 1.0f);
        mConfig.preset = "Custom";
    } else if (key == "accentRed") {
        mConfig.accentRed = parseIntToken(value, mConfig.accentRed, 0, 255);
        mConfig.colorMode = "Custom Color";
        mConfig.preset = "Custom";
    } else if (key == "accentGreen") {
        mConfig.accentGreen = parseIntToken(value, mConfig.accentGreen, 0, 255);
        mConfig.colorMode = "Custom Color";
        mConfig.preset = "Custom";
    } else if (key == "accentBlue") {
        mConfig.accentBlue = parseIntToken(value, mConfig.accentBlue, 0, 255);
        mConfig.colorMode = "Custom Color";
        mConfig.preset = "Custom";
    } else if (key == "accentAlpha") {
        mConfig.accentAlpha = parseIntToken(value, mConfig.accentAlpha, 0, 255);
        mConfig.preset = "Custom";
    } else if (key == "smoothAnimation") {
        mConfig.smoothAnimation = parseBoolToken(value, mConfig.smoothAnimation);
    } else if (key == "animationSpeed") {
        mConfig.animationSpeed = parseFloatToken(value, mConfig.animationSpeed, 4.0f, 40.0f);
    } else if (key == "directionalShading") {
        mConfig.directionalShading = parseBoolToken(value, mConfig.directionalShading);
    } else if (key == "showOrientationGuides") {
        mConfig.showOrientationGuides = parseBoolToken(value, mConfig.showOrientationGuides);
    } else if (key == "subtleBreathingPulse") {
        mConfig.subtleBreathingPulse = parseBoolToken(value, mConfig.subtleBreathingPulse);
    } else if (key == "hideOnInteractiveWithoutSneak") {
        mConfig.hideOnInteractiveWithoutSneak = parseBoolToken(value, mConfig.hideOnInteractiveWithoutSneak);
    } else if (key == "resetDefaults") {
        mConfig = ModConfig{};
    }

    saveLocked();
}

std::string ConfigManager::serializeJson(const ModConfig& cfg) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "{\n"
        << "  \"schemaVersion\": " << cfg.schemaVersion << ",\n"
        << "  \"enabled\": " << (cfg.enabled ? "true" : "false") << ",\n"
        << "  \"preset\": \"" << jsonEscape(cfg.preset) << "\",\n"
        << "  \"previewColor\": \"" << cfg.previewColorHex() << "\",\n"
        << "  \"renderMode\": \"" << jsonEscape(cfg.renderMode) << "\",\n"
        << "  \"colorMode\": \"" << jsonEscape(cfg.colorMode) << "\",\n"
        << "  \"fillOpacity\": " << cfg.fillOpacity << ",\n"
        << "  \"wireframeOpacity\": " << cfg.wireframeOpacity << ",\n"
        << "  \"accentRed\": " << cfg.accentRed << ",\n"
        << "  \"accentGreen\": " << cfg.accentGreen << ",\n"
        << "  \"accentBlue\": " << cfg.accentBlue << ",\n"
        << "  \"accentAlpha\": " << cfg.accentAlpha << ",\n"
        << "  \"smoothAnimation\": " << (cfg.smoothAnimation ? "true" : "false") << ",\n"
        << "  \"animationSpeed\": " << cfg.animationSpeed << ",\n"
        << "  \"directionalShading\": " << (cfg.directionalShading ? "true" : "false") << ",\n"
        << "  \"showOrientationGuides\": " << (cfg.showOrientationGuides ? "true" : "false") << ",\n"
        << "  \"subtleBreathingPulse\": " << (cfg.subtleBreathingPulse ? "true" : "false") << ",\n"
        << "  \"hideOnInteractiveWithoutSneak\": " << (cfg.hideOnInteractiveWithoutSneak ? "true" : "false") << "\n"
        << "}\n";
    return oss.str();
}

ModConfig ConfigManager::parseJson(std::string_view jsonText) {
    ModConfig cfg{};
    std::string_view tok;

    if (extractRawField(jsonText, "enabled", tok)) cfg.enabled = parseBoolToken(tok, cfg.enabled);
    if (extractRawField(jsonText, "preset", tok)) cfg.preset = std::string(tok);
    if (extractRawField(jsonText, "renderMode", tok) &&
        (tok == "Shaded + Wireframe" || tok == "Wireframe Only" || tok == "Translucent Fill")) {
        cfg.renderMode = std::string(tok);
    }
    if (extractRawField(jsonText, "colorMode", tok)) {
        cfg.colorMode = (tok == "Material Adaptive") ? "Material Adaptive" : "Custom Color";
    }
    if (extractRawField(jsonText, "fillOpacity", tok)) cfg.fillOpacity = parseFloatToken(tok, cfg.fillOpacity, 0.0f, 1.0f);
    if (extractRawField(jsonText, "wireframeOpacity", tok)) cfg.wireframeOpacity = parseFloatToken(tok, cfg.wireframeOpacity, 0.0f, 1.0f);
    if (extractRawField(jsonText, "accentRed", tok)) cfg.accentRed = parseIntToken(tok, cfg.accentRed, 0, 255);
    if (extractRawField(jsonText, "accentGreen", tok)) cfg.accentGreen = parseIntToken(tok, cfg.accentGreen, 0, 255);
    if (extractRawField(jsonText, "accentBlue", tok)) cfg.accentBlue = parseIntToken(tok, cfg.accentBlue, 0, 255);
    if (extractRawField(jsonText, "accentAlpha", tok)) cfg.accentAlpha = parseIntToken(tok, cfg.accentAlpha, 0, 255);
    if (extractRawField(jsonText, "previewColor", tok)) {
        int r = cfg.accentRed, g = cfg.accentGreen, b = cfg.accentBlue, a = cfg.accentAlpha;
        if (parseHexColor(tok, r, g, b, a)) {
            cfg.accentRed = r;
            cfg.accentGreen = g;
            cfg.accentBlue = b;
            cfg.accentAlpha = a;
        }
    }
    if (extractRawField(jsonText, "smoothAnimation", tok)) cfg.smoothAnimation = parseBoolToken(tok, cfg.smoothAnimation);
    if (extractRawField(jsonText, "animationSpeed", tok)) cfg.animationSpeed = parseFloatToken(tok, cfg.animationSpeed, 4.0f, 40.0f);
    if (extractRawField(jsonText, "directionalShading", tok)) cfg.directionalShading = parseBoolToken(tok, cfg.directionalShading);
    if (extractRawField(jsonText, "showOrientationGuides", tok)) cfg.showOrientationGuides = parseBoolToken(tok, cfg.showOrientationGuides);
    if (extractRawField(jsonText, "subtleBreathingPulse", tok)) cfg.subtleBreathingPulse = parseBoolToken(tok, cfg.subtleBreathingPulse);
    if (extractRawField(jsonText, "hideOnInteractiveWithoutSneak", tok)) cfg.hideOnInteractiveWithoutSneak = parseBoolToken(tok, cfg.hideOnInteractiveWithoutSneak);

    return cfg;
}

bool ConfigManager::loadFromFile(const std::filesystem::path& path) {
    if (path.empty()) return false;
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string body = ss.str();
    if (body.empty()) return false;

    const ModConfig loaded = parseJson(body);
    std::lock_guard<std::mutex> lock(mMutex);
    mConfig = loaded;
    return true;
}

bool ConfigManager::saveToFile(const std::filesystem::path& path) const {
    if (path.empty()) return false;
    ModConfig copy;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        copy = mConfig;
    }
    return writeTextFileRobust(path, serializeJson(copy));
}

void ConfigManager::saveLocked() const {
    if (mFilePath.empty()) return;
    (void)writeTextFileRobust(mFilePath, serializeJson(mConfig));
}

} // namespace placepreview
