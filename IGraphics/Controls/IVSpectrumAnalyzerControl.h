/*
  ==============================================================================
  
  This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.
  
  See LICENSE.txt for  more info.
  
  ==============================================================================
  */

 #pragma once

 /**
  * @file
  * @ingroup Controls
  * @copydoc IVSpectrumAnalyzerControl
  */

 #include "IControl.h"
 #include "ISender.h"
 #include "IPlugStructs.h"

 #define FFTSIZE_VA_LIST "128", "256", "512", "1024", "2048", "4096"
 #define FFTWINDOWS_VA_LIST "Hann", "BlackmanHarris", "Hamming", "Flattop", "Rectangular"

 BEGIN_IPLUG_NAMESPACE
 BEGIN_IGRAPHICS_NAMESPACE

 /** Vectorial multi-channel capable spectrum analyzer control
  * @ingroup IControls
  * Derived from work by Alex Harker and Matthew Witmer
  */
 template <int MAXNC = 1, int MAX_FFT_SIZE = 4096>
 class IVSpectrumAnalyzerControl : public IControl
                                 , public IVectorBase
 {
   using TDataPacket = std::array<float, MAX_FFT_SIZE>;

 public:
   /** Constructs an IVSpectrumAnalyzerControl
    * @param bounds The rectangular area that the control occupies
    * @param label A CString to label the control
    * @param style, /see IVStyle */
   IVSpectrumAnalyzerControl(const IRECT& bounds, const char* label = "", const IVStyle& style = DEFAULT_STYLE,
     std::initializer_list<IColor> colors = {COLOR_BLACK})
   : IControl(bounds)
   , IVectorBase(style)
   , mChannelColors(colors)
   {
     AttachIControl(this, label);

     SetFreqRange(20.f, 20000.f, 44100.f);
     SetDBRange(-90.f, 0.f);
   }

   void Draw(IGraphics& g) override
   {
     DrawBackground(g, mRECT);
     //DrawWidget(g);
     //DrawTopLines(g);
     //DrawRectangles(g);
     DrawFilledLines(g);
     DrawLabel(g);
     
     DrawMarkers(g);

     if (mStyle.drawFrame)
       g.DrawRect(GetColor(kFR), mWidgetBounds, &mBlend, mStyle.frameThickness);
   }

   void DrawMarkers(IGraphics& g)
   {
     DrawFreqMarkers(g);
     DrawPowerMarkers(g);
   }
   
   WDL_String GetFrequencyFormatted(const float freq)
   {
     WDL_String str;
     if(freq >= 1000.f)
     {
       str.SetFormatted(12, "%i kHz", static_cast<int>(freq/1000.f));
     }
     else
     {
       str.SetFormatted(12, "%i Hz", static_cast<int>(freq));
     }
     
     return str;
   }
   
   void DrawFreqMarkers(IGraphics& g)
   {
     WDL_String measuringString;
     measuringString.SetFormatted(12, "%i kHz", static_cast<int>(22.5f));
     
     IRECT textRect;
     GetUI()->MeasureText(mStyle.valueText, measuringString.Get(), textRect);

     float candidateFreqs[] = {10.f, 50.f, 100.f, 250.f, 1000.f, 5000.f, 10000.f, 20000.f, 25000.f};
     std::vector<float> selectedFreqs;
     
     selectedFreqs.push_back(mFreqLo);
     
     for(float freq: candidateFreqs)
     {
       if(freq <= mFreqLo) continue;
       if(freq >= mFreqHi) break;
       
       selectedFreqs.push_back(freq);
     }
     
     selectedFreqs.push_back(mFreqHi);
     
     const int numberOfFreqs = static_cast<int>(selectedFreqs.size());
     
     IRECT freqTextStrip = mWidgetBounds.GetFromBottom(textRect.H());
     
     int column = 0;
     for(auto freq: selectedFreqs)
     {
       // TODO: This prototype spread frequencies linearly, they should be aligned to their
       // TODO: respective bins taking care of mFreqLo and mFreqHi that may be drawn separately
       IRECT textBox = freqTextStrip.GetGridCell(0, column++, 1, numberOfFreqs);
       g.DrawText(DEFAULT_TEXT, GetFrequencyFormatted(freq).Get(), textBox);
     }

   }

   void DrawPowerMarkers(IGraphics& g)
   {
     // TODO:
     
   }
   
   void DrawFilledLines(IGraphics& g)
   {
     for (auto c = 0; c < MAXNC; c++)
     {
       IColor fillColor = IColor::LinearInterpolateBetween(
          COLOR_WHITE, mChannelColors[c], 0.6f);

       size_t nLines = mYPoints[c].size();

       // Calculate each line width
       float lineWidth = mWidgetBounds.W() / nLines;

       float xLo = mWidgetBounds.L;
       float xHi = xLo + lineWidth;

       for(float yPoint: mYPoints[c])
       {
         float y = mWidgetBounds.B - mWidgetBounds.H() * yPoint;

         g.DrawHorizontalLine(mChannelColors[c], y, xLo, xHi);
         g.FillRect(fillColor, IRECT{xLo, y, xHi + 1.0f, mWidgetBounds.B});

         xLo = xHi;
         xHi += lineWidth;
       }
     }
   }

   void DrawTopLines(IGraphics& g)
   {
     for (auto c = 0; c < MAXNC; c++)
     {
       size_t nLines = mYPoints[c].size();

       // Calculate each line width
       float lineWidth = mWidgetBounds.W() / nLines;

       float xLo = mWidgetBounds.L;
       float xHi = xLo + lineWidth;

       for(float yPoint: mYPoints[c])
       {
         float y = mWidgetBounds.B - mWidgetBounds.H() * yPoint;

         g.DrawHorizontalLine(mChannelColors[c], y, xLo, xHi);
         xLo = xHi;
         xHi += lineWidth;
       }
     }
   }

   void DrawRectangles(IGraphics& g)
   {
     for (auto c = 0; c < MAXNC; c++)
     {
       size_t nLines = mYPoints[c].size();

       // Calculate each line width
       float lineWidth = mWidgetBounds.W() / nLines;

       float xLo = mWidgetBounds.L;
       float xHi = xLo + lineWidth;

       for(float yPoint: mYPoints[c])
       {
         float y = mWidgetBounds.B - mWidgetBounds.H() * yPoint;

         g.FillRect(mChannelColors[c], IRECT{xLo, y, xHi, mWidgetBounds.B});

         xLo = xHi;
         xHi += lineWidth;
       }
     }
   }


   void DrawWidget(IGraphics& g) override
   {
     for (auto c = 0; c < MAXNC; c++)
     {
       g.DrawData(mChannelColors[c], mWidgetBounds, mYPoints[c].data(), NPoints(c), mXPoints[c].data());
     }
   }

   void OnResize() override
   {
     SetTargetRECT(MakeRects(mRECT));
     SetDirty(false);
   }

   void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
   {
     if (!IsDisabled() && msgTag == ISender<>::kUpdateMessage)
     {
       IByteStream stream(pData, dataSize);

       int pos = 0;
       ISenderData<MAXNC, TDataPacket> d;
       pos = stream.Get(&d, pos);

       for (auto c = d.chanOffset; c < (d.chanOffset + d.nChans); c++)
       {
         //CalculatePoints(c, d.vals[c].data(), (mFFTSize / 2) + 1);
         //CalculatePointsLinearFrequency(c, d.vals[c].data(), (mFFTSize / 2) + 1);
         //CalculateTestLines(c, d.vals[c].data(), (mFFTSize / 2) + 1);
         CalculateLines(c, d.vals[c].data(), (mFFTSize / 2) + 1);
       }

       SetDirty(false);
     }
   }

   void SetFFTSize(int fftSize)
   {
     assert(fftSize > 0);
     assert(fftSize <= MAX_FFT_SIZE);
     mFFTSize = fftSize;

     for (auto c = 0; c < MAXNC; c++)
     {
       mXPoints[c].clear();
       mYPoints[c].clear();
     }
   }

   void SetFreqRange(float freqLo, float freqHi, float sampleRate)
   {
     auto nyquist = sampleRate / 2.f;
     assert(freqHi < nyquist);
     assert(freqLo >= 0.f);
     assert(freqHi > freqLo);
     
     mFreqLo = freqLo;
     mFreqHi = freqHi;

     mLogXLo = std::logf(freqLo / nyquist);
     mLogXHi = std::logf(freqHi / nyquist);
   }

   void SetDBRange(float dbLo, float dbHi)
   {
     mLogYLo = std::logf(std::powf(10.f, dbLo / 10.0));
     mLogYHi = std::logf(std::powf(10.f, dbHi / 10.0));
   }

 protected:
   std::vector<IColor> mChannelColors;
   int mFFTSize = 1024;

   void CalculateLines(int ch, const float* powerSpectrum, int size)
   {
     mYPoints[ch].reserve(size);
     mYPoints[ch].clear();

     for(int bin = 0; bin < size; bin++)
     {
       float power = powerSpectrum[bin];

       // Calcular log de power y ubicar coordenada Y dentro de los limites del componente.
       float yNorm = CalcYNorm(power);
       //float yPosition = 1.0 - (YCalc(yNorm) / mWidgetBounds.H());

       mYPoints[ch].push_back(yNorm);
     }
   }

   void CalculateTestLines(int ch, const float* powerSpectrum, int size)
   {
     mYPoints[ch].clear();
     mYPoints[ch].push_back(0.1f);
     mYPoints[ch].push_back(0.8f);
     mYPoints[ch].push_back(0.5f);
   }

   void CalculatePointsLinearFrequency(int ch, const float* powerSpectrum, int size)
   {
     mXPoints[ch].reserve(size);
     mXPoints[ch].clear();
     mYPoints[ch].reserve(size);
     mYPoints[ch].clear();

     for(int bin = 0; bin < size; bin++)
     {
       float power = powerSpectrum[bin];
       
       // Obtener posicion lineal del bin
       float xPosition = LinearBinPosition(bin, size);
       
       // Calcular log de power y ubicar coordenada Y dentro de los limites del componente.
       float yNorm = CalcYNorm(power);
       float yPosition = 1.0 - (YCalc(yNorm) / mWidgetBounds.H());
       
       mXPoints[ch].push_back(xPosition);
       mYPoints[ch].push_back(yPosition);
     }
     
   }
   
   void CalculatePoints(int ch, const float* powerSpectrum, int size)
   {
     mXPoints[ch].reserve(size);
     mYPoints[ch].reserve(size);
     mXPoints[ch].clear();
     mYPoints[ch].clear();

     float xRecip = 1.f / static_cast<float>(size);
     float xAdvance = mOptimiseX / mWidgetBounds.W();
     float xPrev = 0.f;

     float ym2 = CalcYNorm(powerSpectrum[1]);
     float ym1 = CalcYNorm(powerSpectrum[0]);
     float yp0 = CalcYNorm(powerSpectrum[1]);
     float yp1 = 0.0;

     // Don't use the DC bin

     unsigned long bin = 1;

     // Calculate Curve

     auto InterpolateCubic = [](const float& x, const float& y0, const float& y1, const float& y2, const float& y3) {
       // N.B. - this is currently a high-quality cubic hermite

       const auto c0 = y1;
       const auto c1 = 0.5f * (y2 - y0);
       const auto c2 = y0 - 2.5f * y1 + y2 + y2 - 0.5f * y3;
       const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

       return (((c3 * x + c2) * x + c1) * x + c0);
     };

     for (; bin < size; bin++)
     {
       const auto N = NPoints(ch);
       float x = CalcXNorm(bin * xRecip);

       // Add cubic smoothing if desired

       if (bin + 1 < size)
       {
         yp1 = CalcYNorm(powerSpectrum[bin+1]);

         if (mSmoothX)
         {
           float xInterp = 1.0 / (x - xPrev);

           for (float xS = xPrev + xAdvance; xS < x - xAdvance; xS += xAdvance)
             AddPoint(ch, IVec2(xS, InterpolateCubic((xS - xPrev) * xInterp, ym2, ym1, yp0, yp1)));
         }
       }

       AddPoint(ch, IVec2(x, yp0));

       ym2 = ym1;
       ym1 = yp0;
       yp0 = yp1;

       if (N && (XCalc(mXPoints[ch][N]) - XCalc(mXPoints[ch][N - 1])) < mOptimiseX)
         break;

       xPrev = x;
     }

     while (bin < size)
     {
       IVec2 minPoint(CalcXNorm(bin * xRecip), powerSpectrum[bin]);
       IVec2 maxPoint = minPoint;

       float x = XCalc(minPoint.x);
       float xNorm = CalcXNorm(++bin * xRecip);

       while (((XCalc(xNorm) - x) < mOptimiseX) && bin < size)
       {
         if (powerSpectrum[bin] < minPoint.y)
           minPoint = IVec2(xNorm, powerSpectrum[bin]);
         if (powerSpectrum[bin] > maxPoint.y)
           maxPoint = IVec2(xNorm, powerSpectrum[bin]);
         xNorm = CalcXNorm(++bin * xRecip);
       }

       if (minPoint.x < maxPoint.x)
         ConvertAndAddPoints(ch, minPoint, maxPoint);
       else
         ConvertAndAddPoints(ch, maxPoint, minPoint);
     }
   }

   void AddPoint(int ch, const IVec2& p)
   {
     mXPoints[ch].push_back(p.x);
     mYPoints[ch].push_back(p.y);
   }

   void ConvertAndAddPoints(int ch, IVec2& p1, IVec2& p2)
   {
     p1.y = CalcYNorm(p1.y);
     p2.y = CalcYNorm(p2.y);

     AddPoint(ch, p1);
     AddPoint(ch, p2);
   }

   float XCalc(float xNorm) const { return mWidgetBounds.L + (mWidgetBounds.W() * xNorm); }
   float YCalc(float yNorm) const { return mWidgetBounds.B - (mWidgetBounds.H() * yNorm); }
   float CalcXNorm(float x) const { return (std::logf(x) - mLogXLo) / (mLogXHi - mLogXLo); }
   float CalcYNorm(float y) const { return (std::logf(y) - mLogYLo) / (mLogYHi - mLogYLo); }
   int NPoints(int ch) const { return static_cast<int>(mXPoints[ch].size()); }

   
   float LinearBinPosition(int bin, int size) const {
     float binAsFloat = static_cast<float>(bin);
     float sizeAsFloat = static_cast<float>(size);
     float xNorm = binAsFloat/sizeAsFloat;
     return xNorm;
     //return mWidgetBounds.L + (mWidgetBounds.W() * xNorm);
   }

   
   float mOptimiseX = 1.f;
   float mSmoothX = 1.f;

   float mLogXLo;
   float mLogXHi;
   float mLogYLo;
   float mLogYHi;

   float mFreqLo = 0.f;
   float mFreqHi = 20000.f;

   std::array<std::vector<float>, MAXNC> mXPoints;
   std::array<std::vector<float>, MAXNC> mYPoints;
      
 };

 END_IGRAPHICS_NAMESPACE
 END_IPLUG_NAMESPACE
