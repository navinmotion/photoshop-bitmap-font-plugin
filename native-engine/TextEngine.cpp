#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <map>
#include <sstream>
#include <fstream> 

// Dependencies
#include <uWebSockets/App.h>
#include <nlohmann/json.hpp>

// FreeType & HarfBuzz
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include <zlib.h>
#include "base64.h" 

using json = nlohmann::json;

FT_Library ftLibrary;

// --- MAXRECTS BIN PACKER ---
struct Rect {
    int x, y, w, h;
};

class MaxRectsBinPack {
public:
    int binWidth = 0;
    int binHeight = 0;
    std::vector<Rect> freeRects;

    void Init(int width, int height) {
        binWidth = width;
        binHeight = height;
        freeRects.clear();
        freeRects.push_back({0, 0, width, height});
    }

    Rect Insert(int width, int height) {
        Rect bestNode = {0, 0, 0, 0};
        int bestShortSideFit = std::numeric_limits<int>::max();
        int bestLongSideFit = std::numeric_limits<int>::max();

        for (size_t i = 0; i < freeRects.size(); ++i) {
            Rect& free = freeRects[i];
            if (free.w >= width && free.h >= height) {
                int leftoverHoriz = abs(free.w - width);
                int leftoverVert = abs(free.h - height);
                int shortSideFit = std::min(leftoverHoriz, leftoverVert);
                int longSideFit = std::max(leftoverHoriz, leftoverVert);

                if (shortSideFit < bestShortSideFit || (shortSideFit == bestShortSideFit && longSideFit < bestLongSideFit)) {
                    bestNode.x = free.x;
                    bestNode.y = free.y;
                    bestNode.w = width;
                    bestNode.h = height;
                    bestShortSideFit = shortSideFit;
                    bestLongSideFit = longSideFit;
                }
            }
        }

        if (bestNode.w == 0) return bestNode;

        int numRectsToProcess = (int)freeRects.size();
        for(int i = 0; i < numRectsToProcess; ++i) {
            if (SplitFreeNode(freeRects[i], bestNode.x, bestNode.y, width, height)) {
                freeRects.erase(freeRects.begin() + i);
                --i;
                --numRectsToProcess;
            }
        }
        PruneFreeList();
        return bestNode;
    }

private:
    bool SplitFreeNode(Rect freeNode, int usedX, int usedY, int usedW, int usedH) {
        if (usedX >= freeNode.x + freeNode.w || usedX + usedW <= freeNode.x ||
            usedY >= freeNode.y + freeNode.h || usedY + usedH <= freeNode.y)
            return false;

        if (usedX < freeNode.x + freeNode.w && usedX + usedW > freeNode.x) {
            if (usedY > freeNode.y && usedY < freeNode.y + freeNode.h) {
                Rect newNode = freeNode;
                newNode.h = usedY - newNode.y;
                freeRects.push_back(newNode);
            }
            if (usedY + usedH < freeNode.y + freeNode.h) {
                Rect newNode = freeNode;
                newNode.y = usedY + usedH;
                newNode.h = freeNode.y + freeNode.h - (usedY + usedH);
                freeRects.push_back(newNode);
            }
        }

        if (usedY < freeNode.y + freeNode.h && usedY + usedH > freeNode.y) {
            if (usedX > freeNode.x && usedX < freeNode.x + freeNode.w) {
                Rect newNode = freeNode;
                newNode.w = usedX - newNode.x;
                freeRects.push_back(newNode);
            }
            if (usedX + usedW < freeNode.x + freeNode.w) {
                Rect newNode = freeNode;
                newNode.x = usedX + usedW;
                newNode.w = freeNode.x + freeNode.w - (usedX + usedW);
                freeRects.push_back(newNode);
            }
        }
        return true;
    }

    void PruneFreeList() {
        for(size_t i = 0; i < freeRects.size(); ++i) {
            for(size_t j = i+1; j < freeRects.size(); ++j) {
                if (IsContainedIn(freeRects[i], freeRects[j])) {
                    freeRects.erase(freeRects.begin() + i);
                    --i; break;
                }
                if (IsContainedIn(freeRects[j], freeRects[i])) {
                    freeRects.erase(freeRects.begin() + j);
                    --j;
                }
            }
        }
    }

    bool IsContainedIn(const Rect& a, const Rect& b) {
        return a.x >= b.x && a.y >= b.y && (a.x + a.w) <= (b.x + b.w) && (a.y + a.h) <= (b.y + b.h);
    }
};

// --- PNG WRITER ---
void write_chunk(std::vector<unsigned char>& out, const char* type, const std::vector<unsigned char>& data) {
    uint32_t len = (uint32_t)data.size();
    unsigned char len_bytes[4] = { (unsigned char)((len >> 24) & 0xFF), (unsigned char)((len >> 16) & 0xFF), (unsigned char)((len >> 8) & 0xFF), (unsigned char)(len & 0xFF) };
    out.insert(out.end(), len_bytes, len_bytes + 4);
    out.insert(out.end(), type, type + 4);
    if (!data.empty()) out.insert(out.end(), data.begin(), data.end());
    uint32_t crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char*)type, 4);
    if (!data.empty()) crc = crc32(crc, data.data(), (uInt)data.size());
    unsigned char crc_bytes[4] = { (unsigned char)((crc >> 24) & 0xFF), (unsigned char)((crc >> 16) & 0xFF), (unsigned char)((crc >> 8) & 0xFF), (unsigned char)(crc & 0xFF) };
    out.insert(out.end(), crc_bytes, crc_bytes + 4);
}

void writePNG(std::vector<unsigned char>& out, const std::vector<unsigned char>& rgba, int w, int h) {
    const unsigned char signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    out.insert(out.end(), signature, signature + 8);
    std::vector<unsigned char> ihdr(13);
    ihdr[0] = (w >> 24) & 0xFF; ihdr[1] = (w >> 16) & 0xFF; ihdr[2] = (w >> 8) & 0xFF; ihdr[3] = w & 0xFF;
    ihdr[4] = (h >> 24) & 0xFF; ihdr[5] = (h >> 16) & 0xFF; ihdr[6] = (h >> 8) & 0xFF; ihdr[7] = h & 0xFF;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    write_chunk(out, "IHDR", ihdr);
    std::vector<unsigned char> raw_data;
    raw_data.reserve(h * (w * 4 + 1));
    for (int y = 0; y < h; ++y) {
        raw_data.push_back(0); 
        const unsigned char* row = &rgba[y * w * 4];
        raw_data.insert(raw_data.end(), row, row + (w * 4));
    }
    uLongf destLen = compressBound((uLong)raw_data.size());
    std::vector<unsigned char> idat(destLen);
    if (compress(idat.data(), &destLen, raw_data.data(), (uLong)raw_data.size()) == Z_OK) {
        idat.resize(destLen);
        write_chunk(out, "IDAT", idat);
    }
    write_chunk(out, "IEND", {});
}

// --- UTF-8 HELPER ---
uint32_t utf8_next(const char** p) {
    unsigned char c = **p;
    if (c == 0) return 0;
    uint32_t code = 0;
    int len = 0;
    if (c < 0x80) { code = c; len = 1; }
    else if ((c & 0xE0) == 0xC0) { code = c & 0x1F; len = 2; }
    else if ((c & 0xF0) == 0xE0) { code = c & 0x0F; len = 3; }
    else if ((c & 0xF8) == 0xF0) { code = c & 0x07; len = 4; }
    (*p)++;
    for (int i = 1; i < len; ++i) { code = (code << 6) | (**p & 0x3F); (*p)++; }
    return code;
}

// --- CORE GENERATOR ---
MaxRectsBinPack packer;

struct AtlasResult {
    std::string base64Png;
    std::string fntData;
};

struct GlyphData {
    uint32_t cp;
    int w, h, xadv, xoff, yoff;
    std::vector<unsigned char> buffer; 
};

// MODIFIED: Added global metrics
AtlasResult generateAtlas(const std::string& text, const std::string& fontInput, int fontSize, int atlasWidth, int padding, int spacing, bool autoPack, std::string packMode, int effectPadding, int gXAdv, int gXOff, int gYOff) {
    
    FT_Face face = nullptr;
    std::string fontName = "Custom";
    bool loaded = false;

    // Load Font logic...
    bool looksLikePath = (fontInput.find('/') != std::string::npos) || 
                         (fontInput.find('\\') != std::string::npos) || 
                         (fontInput.find('.') != std::string::npos);

    if (looksLikePath) {
        std::cout << "Processing Font Request: [" << fontInput << "]" << std::endl;
        int error = FT_New_Face(ftLibrary, fontInput.c_str(), 0, &face);
        if (error == 0) {
            loaded = true;
            size_t lastSlash = fontInput.find_last_of("/\\");
            fontName = (lastSlash == std::string::npos) ? fontInput : fontInput.substr(lastSlash + 1);
        } else {
            std::cerr << "FT Error " << error << " loading: " << fontInput << std::endl;
        }
    }

    if (!loaded) {
        std::string fontPath = "C:/Windows/Fonts/arial.ttf";
        fontName = fontInput;
        if (fontInput == "Times New Roman") fontPath = "C:/Windows/Fonts/times.ttf";
        else if (fontInput == "Courier New") fontPath = "C:/Windows/Fonts/cour.ttf";
        else if (fontInput == "Impact") fontPath = "C:/Windows/Fonts/impact.ttf";
        else if (fontInput == "Arial") fontPath = "C:/Windows/Fonts/arial.ttf";
        
        if (FT_New_Face(ftLibrary, fontPath.c_str(), 0, &face) == 0) loaded = true;
        else if (FT_New_Face(ftLibrary, "C:/Windows/Fonts/arial.ttf", 0, &face) == 0) loaded = true;
    }

    if (!loaded || !face) return {"", ""};

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    std::set<uint32_t> uniqueCodepoints;
    const char* ptr = text.c_str();
    while (uint32_t cp = utf8_next(&ptr)) uniqueCodepoints.insert(cp);

    std::vector<GlyphData> glyphs;
    long long totalArea = 0;
    int maxGlyphW = 0;
    int maxGlyphH = 0;
    
    for (uint32_t cp : uniqueCodepoints) {
        if (FT_Load_Char(face, cp, FT_LOAD_RENDER)) continue;
        FT_Bitmap& bmp = face->glyph->bitmap;
        GlyphData g;
        g.cp = cp;
        g.w = bmp.width;
        g.h = bmp.rows;
        g.xadv = face->glyph->advance.x >> 6;
        g.xoff = face->glyph->bitmap_left;
        g.yoff = (int)((face->size->metrics.ascender >> 6) - face->glyph->bitmap_top);
        
        if (g.w > 0 && g.h > 0) {
            g.buffer.assign(bmp.buffer, bmp.buffer + (g.w * g.h));
            int effectiveW = g.w + (padding*2) + (effectPadding*2) + spacing;
            int effectiveH = g.h + (padding*2) + (effectPadding*2) + spacing;
            totalArea += (effectiveW * effectiveH);
            
            if (effectiveW > maxGlyphW) maxGlyphW = effectiveW;
            if (effectiveH > maxGlyphH) maxGlyphH = effectiveH;
        }
        glyphs.push_back(g);
    }

    std::sort(glyphs.begin(), glyphs.end(), [](const GlyphData& a, const GlyphData& b) {
        return a.h > b.h;
    });

    int finalW = atlasWidth;
    int finalH = atlasWidth;
    
    if (autoPack) {
        int minSideByArea = (int)std::ceil(std::sqrt(totalArea));
        int startDim = std::max({maxGlyphW, maxGlyphH, minSideByArea});
        
        if (packMode == "pot") {
            int pot = 16;
            while(pot < startDim) pot *= 2;
            finalW = pot; finalH = pot;
        } else {
            if (startDim % 4 != 0) startDim += (4 - (startDim % 4));
            finalW = startDim; finalH = startDim;
        }
    }
    
    std::vector<unsigned char> atlasPixels(finalW * finalH * 4);
    for(size_t i=0; i<atlasPixels.size(); i+=4) {
        atlasPixels[i] = 255;   // R
        atlasPixels[i+1] = 255; // G
        atlasPixels[i+2] = 255; // B
        atlasPixels[i+3] = 0;   // A
    }

    bool success = false;
    std::map<uint32_t, Rect> placedRects;

    while (!success && finalW <= 8192) {
        packer.Init(finalW, finalH);
        placedRects.clear();
        bool fits = true;
        
        if(atlasPixels.size() != finalW * finalH * 4) {
            atlasPixels.assign(finalW * finalH * 4, 0);
            for(size_t i=0; i<atlasPixels.size(); i+=4) {
                atlasPixels[i] = 255;
                atlasPixels[i+1] = 255;
                atlasPixels[i+2] = 255;
                atlasPixels[i+3] = 0;
            }
        }
        
        long long packedArea = 0;

        for (auto& g : glyphs) {
            if (g.w == 0 || g.h == 0) continue; 

            int packW = g.w + (padding * 2) + (effectPadding * 2) + spacing;
            int packH = g.h + (padding * 2) + (effectPadding * 2) + spacing;

            Rect r = packer.Insert(packW, packH);
            if (r.w == 0) { fits = false; break; }
            
            packedArea += (packW * packH);

            r.x += (padding + effectPadding);
            r.y += (padding + effectPadding);
            r.w = g.w;
            r.h = g.h;
            placedRects[g.cp] = r;
        }

        if (fits) {
            success = true;
            double efficiency = (double)packedArea / (double)(finalW * finalH) * 100.0;
            std::cout << "Packed " << glyphs.size() << " glyphs into " << finalW << "x" << finalH 
                      << " (Eff: " << (int)efficiency << "%)" << std::endl;
        } else {
            if (!autoPack) break; 
            if (packMode == "pot") { finalW *= 2; finalH *= 2; }
            else { finalW += 32; finalH += 32; }
        }
    }

    if (!success) std::cerr << "Packing failed." << std::endl;

    // Render
    std::stringstream fnt;
    int reportPad = padding; 
    
    fnt << "info face=\"" << fontName << "\" size=" << fontSize << " bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=1 aa=1 padding=" << reportPad << "," << reportPad << "," << reportPad << "," << reportPad << " spacing=" << spacing << "," << spacing << " outline=0\n";
    fnt << "common lineHeight=" << fontSize << " base=" << (fontSize * 0.8) << " scaleW=" << finalW << " scaleH=" << finalH << " pages=1 packed=0 alphaChnl=0 redChnl=0 greenChnl=0 blueChnl=0\n";
    fnt << "page id=0 file=\"texture.png\"\n";

    std::stringstream charLines;
    int charCount = 0;

    for (auto& g : glyphs) {
        if (g.w == 0 || g.h == 0) {
            // Apply global adjustments even to invisible chars if necessary, mostly advance
            int finalXAdv = g.xadv + gXAdv;
            charLines << "char id=" << g.cp << " x=0 y=0 width=0 height=0 xoffset=0 yoffset=0 xadvance=" << finalXAdv << " page=0 chnl=15\n";
            charCount++;
            continue;
        }

        if (placedRects.find(g.cp) != placedRects.end()) {
            Rect r = placedRects[g.cp];
            
            for (int y = 0; y < g.h; ++y) {
                for (int x = 0; x < g.w; ++x) {
                    unsigned char alpha = g.buffer[y * g.w + x];
                    if (alpha > 0) {
                        int targetIdx = ((r.y + y) * finalW + (r.x + x)) * 4;
                        if (targetIdx < atlasPixels.size()) {
                            atlasPixels[targetIdx + 0] = 255; 
                            atlasPixels[targetIdx + 1] = 255; 
                            atlasPixels[targetIdx + 2] = 255; 
                            atlasPixels[targetIdx + 3] = alpha; 
                        }
                    }
                }
            }
            
            int fnX = r.x - effectPadding;
            int fnY = r.y - effectPadding;
            int fnW = g.w + (effectPadding * 2);
            int fnH = g.h + (effectPadding * 2);
            
            // Apply global adjustments here
            int fnXOff = g.xoff - effectPadding + gXOff;
            int fnYOff = g.yoff - effectPadding + gYOff;
            int finalXAdv = g.xadv + gXAdv;

            charLines << "char id=" << g.cp << " x=" << fnX << " y=" << fnY 
                      << " width=" << fnW << " height=" << fnH 
                      << " xoffset=" << fnXOff << " yoffset=" << fnYOff 
                      << " xadvance=" << finalXAdv << " page=0 chnl=15\n";
            charCount++;
        }
    }

    fnt << "chars count=" << charCount << "\n";
    fnt << charLines.str();
    FT_Done_Face(face);

    std::vector<unsigned char> pngData;
    writePNG(pngData, atlasPixels, finalW, finalH);
    return { base64_encode(pngData.data(), (unsigned int)pngData.size()), fnt.str() };
}

int main() {
    if (FT_Init_FreeType(&ftLibrary)) return 1;

    uWS::App().ws<int>("/*", {
        .compression = uWS::SHARED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024 * 1024,
        .idleTimeout = 60,
        .open = [](auto *ws) { std::cout << "Connected" << std::endl; },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            try {
                auto j = json::parse(message);
                
                std::string text = "ABC";
                if (j.contains("text") && !j["text"].is_null()) text = j["text"];
                std::string font = "Arial";
                if (j.contains("font") && !j["font"].is_null()) font = j["font"];
                int size = 48;
                if (j.contains("size") && !j["size"].is_null()) size = j["size"];
                int width = 512;
                if (j.contains("width") && !j["width"].is_null()) width = j["width"];
                int pad = 2;
                if (j.contains("padding") && !j["padding"].is_null()) pad = j["padding"];
                int spacing = 2;
                if (j.contains("spacing") && !j["spacing"].is_null()) spacing = j["spacing"];
                bool autoPack = false;
                if (j.contains("autoPack") && !j["autoPack"].is_null()) autoPack = j["autoPack"];
                std::string packMode = "pot";
                if (j.contains("packMode") && !j["packMode"].is_null()) packMode = j["packMode"];
                int effectPad = 0;
                if (j.contains("effectPadding") && !j["effectPadding"].is_null()) effectPad = j["effectPadding"];
                
                // Read global metrics
                int gXAdv = 0;
                if (j.contains("globalXAdvance") && !j["globalXAdvance"].is_null()) gXAdv = j["globalXAdvance"];
                int gXOff = 0;
                if (j.contains("globalXOffset") && !j["globalXOffset"].is_null()) gXOff = j["globalXOffset"];
                int gYOff = 0;
                if (j.contains("globalYOffset") && !j["globalYOffset"].is_null()) gYOff = j["globalYOffset"];

                AtlasResult result = generateAtlas(text, font, size, width, pad, spacing, autoPack, packMode, effectPad, gXAdv, gXOff, gYOff);

                json response;
                response["image"] = result.base64Png;
                response["fnt"] = result.fntData;
                
                ws->send(response.dump(), uWS::OpCode::TEXT);
            } catch (std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
    }).listen(4567, [](auto *token) {
        if (token) std::cout << "Listening on port 4567..." << std::endl;
        else std::cerr << "Failed to listen on 4567" << std::endl;
    }).run();

    return 0;
}