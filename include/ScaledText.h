#pragma once
#include "Text.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>

class ScaledText : public Text {
  public:
    ScaledText(std::string Content, int posX, int posY, int TargetWidth)
        : Text{Content, posX, posY, BaseFontSize} {
        int Width;
        TTF_SizeUTF8(mFont, Content.c_str(), &Width, nullptr);

        float Ratio{static_cast<float>(TargetWidth / Width)};
        int   newFontSize = static_cast<int>(BaseFontSize * Ratio);
        if (mFont) {
            TTF_CloseFont(mFont);
        }
        mFont = TTF_OpenFont("fonts/Roboto-VariableFont_wdth,wght.ttf",
                             newFontSize);
        if (!mFont) {
            std::cerr << "Error reloading the font at size " << newFontSize
                      << ": " << TTF_GetError() << "\n";
            return;
        }
        mFontSize = newFontSize;

        SetFontSize(BaseFontSize * Ratio);
        CreateSurface(Content);
    }

  private:
    static constexpr int BaseFontSize{24};
};
