/*
 ==============================================================================
 
 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.
 
 See LICENSE.txt for  more info.
 
 ==============================================================================
 */

#pragma once

#include "ISender.h"
#include "fft.h"

BEGIN_IPLUG_NAMESPACE


constexpr bool is_powerof2(int v) {
    return v && ((v & (v - 1)) == 0);
}

/** ISpectrumSender is a utility class which can be used to defer spectrum data for sending to the GUI
 @tparam MAXNC the max number of channels to use.
 @tparam MAX_FFT_SIZE the size of the buffers to be processed by the FFT.
 @tparam QUEUE_SIZE the max number of buffers to be held in the queue waiting to be processed.
 */
template <int MAXNC = 1, int QUEUE_SIZE = 64, int MAX_FFT_SIZE = 4096>
class ISpectrumSender : public IBufferSender<MAXNC, QUEUE_SIZE, MAX_FFT_SIZE>
{
public:
  using TDataPacket = std::array<float, MAX_FFT_SIZE>;
  using TBufferSender = IBufferSender<MAXNC, QUEUE_SIZE, MAX_FFT_SIZE>;
  
  enum class EWindowType {
    Hann = 0,
    BlackmanHarris,
    Hamming,
    Flattop,
    Rectangular
  };
  
  enum class EOutputType {
    Complex = 0,
    MagPhase,
  };
  
  ISpectrumSender(int fftSize = 1024,
                  int overlap = 2,
                  EWindowType window = EWindowType::Hann,
                  EOutputType outputType = EOutputType::MagPhase)
  : TBufferSender(-std::numeric_limits<double>::infinity(), fftSize)
  , mWindowType(window)
  , mOutputType(outputType)
  {
    // fftSize should be a power of two between 0 and MAX_FFT_SIZE
    // asserts are spread accross lines to help identify bugs.
    assert(is_powerof2(fftSize));
    assert(fftSize > 0);
    assert(fftSize <= MAX_FFT_SIZE);

    WDL_fft_init();
    SetFFTSizeAndOverlap(fftSize, overlap);
  }
  
  void SetFFTSizeAndOverlap(int fftSize, int overlap)
  {
    mOverlap = overlap;
    TBufferSender::SetBufferSize(fftSize);
    SetFFTSize();
    CalculateWindow();
    CalculateScalingFactors();
  }
  
  void SetWindowType(EWindowType windowType)
  {
    mWindowType = windowType;
    CalculateWindow();
  }
  
  void SetOutputType(EOutputType outputType)
  {
    mOutputType = outputType;
  }
  
  /**
   
   */
  void PrepareDataForUI(ISenderData<MAXNC, TDataPacket>& dataPacket) override
  {
    auto fftSize = TBufferSender::GetBufferSize();
    
    for (auto sampleIdx = 0; sampleIdx < fftSize; sampleIdx++)
    {
      for(int stftFrameIdx = 0; stftFrameIdx < mOverlap; stftFrameIdx++)
      {
        // ¿Porqué se cargan los "overlap" frames con la misma información?
        auto& stftFrame = mSTFTFrames[stftFrameIdx];
        
        for (auto ch = 0; ch < MAXNC; ch++)
        {
          auto windowedValue = (float) dataPacket.vals[ch][sampleIdx] * mWindow[stftFrame.pos];
          stftFrame.bins[ch][stftFrame.pos].re = windowedValue;
          stftFrame.bins[ch][stftFrame.pos].im = 0.0f;
        }
        
        stftFrame.pos++;
        
        if(stftFrame.pos >= fftSize)
        {
          stftFrame.pos = 0;
          
          for (auto ch = 0; ch < MAXNC; ch++)
          {
            Permute(ch, stftFrame);
            // Copy fft output into the data packet for the UI
            memcpy(dataPacket.vals[ch].data(), mSTFTOutput[ch].data(), fftSize * sizeof(float));
          }
        }
      }
    }
  }
  
private:
  struct STFTFrame
  {
    int pos;
    std::array<std::array<WDL_FFT_COMPLEX, MAX_FFT_SIZE>, MAXNC> bins;
  };

  void SetFFTSize()
  {
    if (mSTFTFrames.size() != mOverlap)
    {
      mSTFTFrames.resize(mOverlap);
    }
    
    for (auto&& frame : mSTFTFrames)
    {
      for (auto ch = 0; ch < MAXNC; ch++)
      {
        std::fill(frame.bins[ch].begin(), frame.bins[ch].end(), WDL_FFT_COMPLEX{0.0f, 0.0f});
      }
      
      frame.pos = 0;
    }
    
    for (auto ch = 0; ch < MAXNC; ch++)
    {
      std::fill(mSTFTOutput[ch].begin(), mSTFTOutput[ch].end(), 0.0f);
    }
  }
  
  void CalculateWindow()
  {
    const auto fftSize = TBufferSender::GetBufferSize();
    
    const float M = static_cast<float>(fftSize - 1);
    
    switch (mWindowType)
    {
      case EWindowType::Hann:
        for (auto i = 0; i < fftSize; i++) { mWindow[i] = 0.5f * (1.0f - std::cos(PI * 2.0f * i / M)); }
        break;
      case EWindowType::BlackmanHarris:
        for (auto i = 0; i < fftSize; i++) {
          mWindow[i] = 0.35875 - (0.48829f * std::cos(2.0f * PI * i / M)) +
          (0.14128f * std::cos(4.0f * PI * i / M)) -
          (0.01168f * std::cos(6.0f * PI * i / M));
        }
        break;
      case EWindowType::Hamming:
        for (auto i = 0; i < fftSize; i++) { mWindow[i] = 0.54f - 0.46f * std::cos(2.0f * PI * i / M); }
        break;
      case EWindowType::Flattop:
        for (auto i = 0; i < fftSize; i++) {
          mWindow[i] = 0.21557895f - 0.41663158f * std::cos(2.0f * PI * i / M) +
          0.277263158f * std::cos(4.0f * PI * i / M) -
          0.083578947f * std::cos(6.0f * PI * i / M) +
          0.006947368f * std::cos(8.0f * PI * i / M);
        }
        break;
      case EWindowType::Rectangular:
        std::fill(mWindow.begin(), mWindow.end(), 1.0f);
        break;
      default:
        break;
    }
  }
  
  void CalculateScalingFactors()
  {
    const auto fftSize = TBufferSender::GetBufferSize();
    const float M = static_cast<float>(fftSize - 1);
    
    auto scaling = 0.0f;
    
    for (auto i = 0; i < fftSize; i++)
    {
      auto v = 0.5f * (1.0f - std::cos(2.0f * PI * i / M));
      scaling += v;
    }
    
    mScalingFactor = scaling * scaling;
  }
  
  /** Applies fft to a particular frame of the frames vector mSTFTFrames.
   @arg ch the channel to process.
   @arg frameIdx the index of the frame to be processed.
   */
  void Permute(int ch, int frameIdx)
  {
    const auto fftSize = TBufferSender::GetBufferSize();
    WDL_fft(mSTFTFrames[frameIdx].bins[ch].data(), fftSize, false);
    
    if (mOutputType == EOutputType::Complex)
    {
      auto nBins = fftSize/2;
      for (auto i = 0; i < nBins; ++i)
      {
        int sortIdx = WDL_fft_permute(fftSize, i);
        mSTFTOutput[ch][i] = mSTFTFrames[frameIdx].bins[ch][sortIdx].re;
        mSTFTOutput[ch][i + nBins] = mSTFTFrames[frameIdx].bins[ch][sortIdx].im;
      }
    }
    else // magPhase
    {
      for (auto i = 0; i < fftSize; ++i)
      {
        int sortIdx = WDL_fft_permute(fftSize, i);
        auto re = mSTFTFrames[frameIdx].bins[ch][sortIdx].re;
        auto im = mSTFTFrames[frameIdx].bins[ch][sortIdx].im;
        mSTFTOutput[ch][i] = std::sqrt(2.0f * (re * re + im * im) / mScalingFactor);
      }
    }
  }
  
  /** Applies fft to a particular frame.
   @arg ch the channel to process.
   @arg frame the frame to be processed.
   */
  void Permute(int ch, STFTFrame& frame)
  {
    const auto fftSize = TBufferSender::GetBufferSize();
    WDL_fft(frame.bins[ch].data(), fftSize, false);
    
    if (mOutputType == EOutputType::Complex)
    {
      auto nBins = fftSize/2;
      for (auto i = 0; i < nBins; ++i)
      {
        int sortIdx = WDL_fft_permute(fftSize, i);
        mSTFTOutput[ch][i] = frame.bins[ch][sortIdx].re;
        mSTFTOutput[ch][i + nBins] = frame.bins[ch][sortIdx].im;
      }
    }
    else // magPhase
    {
      for (auto i = 0; i < fftSize; ++i)
      {
        int sortIdx = WDL_fft_permute(fftSize, i);
        auto re = frame.bins[ch][sortIdx].re;
        auto im = frame.bins[ch][sortIdx].im;
        mSTFTOutput[ch][i] = std::sqrt(2.0f * (re * re + im * im) / mScalingFactor);
      }
    }
  }
    
  int mOverlap = 2;
  EWindowType mWindowType;
  EOutputType mOutputType;
  std::array<float, MAX_FFT_SIZE> mWindow;
  std::vector<STFTFrame> mSTFTFrames;
  std::array<std::array<float, MAX_FFT_SIZE>, MAXNC> mSTFTOutput;
  float mScalingFactor = 0.0f;
};

END_IPLUG_NAMESPACE
