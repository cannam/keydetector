/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    This file by Daniel Schürmann, based on GetKeyMode from the QM-DSP
    library by Christian Landone and Katy Noland.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "KeyDetectorDaschuer.h"

#include "maths/MathUtilities.h"
#include "base/Pitch.h"
#include "dsp/rateconversion/Decimator.h"
#include "dsp/chromagram/Chromagram.h"

#include <iostream>

#include <cstring>
#include <cstdlib>

//#define DEBUG_KEY_DETECTOR 1

namespace KD {

// Theory: A major third chord consists of a 1st, 3rd, and 5th degrees of a major scale
// while a minor chord is made from the 1st, flatted 3rd, and 5th degrees of a
// major scale.
// The values below are multiplied with the power value of each note in the chroma.
// The values are set in a way to also detect power chords and single notes.
//          c   c#    D   D#    E   F   F#   G    G#   A   A#   B
// Third:  2.4 -1.0 -0.5 -0.3  0.2  1 -1.5  0.7    0  0.2 -0,5 -1
// Power:  1.7 -1.0  0.7 -1.0  0.7  1 -1.0  0.7 -1.0  0.7 -0,5 -1
static const double MajorChord[12] =
{ 1.0, -0.5,  0.0, -0.5,  0.7,  0.0, -0.5,  0.7, -0.5,  0.0, -0.5,  0.0};

static const double MinorChord[12] =
{ 1.0, -0.5,  0.0,  0.7, -0.5,  0.0, -0.5,  0.7,  0.0, -0.5,  0.0, -0.5};


// The chords of C major progressions
// I   C  Tonic
// ii  Dm
// iii Em
// IV  F  Subdominant
// V   G  Dominant
// vi  A
// vii°Bdim (not detected)
static const double ChordToMajorKey[24] =
{ 1.8, -1.0, -1.0, -1.0, -1.0,  1.4, -1.0,  1.4, -1.0, -1.0, -1.0,  0.0,  // Major Chords
 -1.0, -1.0,  0.5, -1.0,  0.5, -1.0, -1.0, -1.0, -1.0,  0.5, -1.0, -1.0}; // minor Chords


// The chords of C minor progressions
// i   Cm  Tonic
// ii° Ddim (not detected)
// III Eb
// iv  Fm  Subdominant
// v   Gm  Dominant
// VI  Ab
// VII Bb/B
// Node: Major chords are used often even in a Minor track, so we weight a minor chord higher
static const double ChordToMinorKey[24] =
{-1.0, -1.0, -1.0,  0.5, -1.0, -1.0, -1.0, -1.0,  0.5, -1.0,  0.5,  0.5,  // Major Chords
  2.0, -1.0,  0.5, -1.0, -1.0,  1.5, -1.0,  1.5, -1.0, -1.0, -1.0, -1.0}; // minor Chords

// For power chords and single notes
static const double NoteToKey[12] =
{ 0.6, -1.0,  0.0, -1.0,  0.0,  0.0, -1.0,  0.0, -1.0,  0.0, -1.0,  0.0};




//static const double MajorScale[12] =
//{ 0.0, -1.0,  0.0, -1.0,  0.0,  0.0, -1.0,  0.0, -1.0,  0.0, -1.0,  0.0};

//static const double MajorGypsyScale[12] =
//{ 0.0,  0.0, -1.0, -1.0,  0.0,  0.0, -1.0,  0.0,  0.0, -1.0, -1.0,  0.0};

static const double MinorScale[12] =
{ 0.0, -1.0,  0.0,  0.0, -1.0,  0.0, -1.0,  0.0,  0.0, -1.0,  0.0, -1.0};

static const double MinorHarmonicScale[12] =
{ 0.0, -1.0,  0.0,  0.0, -1.0,  0.0, -1.0,  0.0,  0.0, -1.0, -1.0,  0.0};

static const double MinorMelodicScale[12] =
{ 0.0, -1.0,  0.0,  0.0, -1.0,  0.0, -1.0,  0.0, -1.0,  0.0, -1.0,  0.0};

static const double MinorGypsyScale[12] =
{ 0.0, -1.0,  0.0,  0.0, -1.0, -1.0,  0.0,  0.0,  0.0, -1.0, -1.0,  0.0};


KeyDetectorDaschuer::KeyDetectorDaschuer(Config config) :
    m_hpcpAverage(config.hpcpAverageWindowLength),
    m_medianAverage(config.medianAverageWindowLength),
    m_chrPointer(0),
    m_decimatedBuffer(0),
    m_chromaBuffer(0),
    m_meanHPCP(0),
    m_inTuneChroma(0),
    m_maxTuneSum(0),
    m_majCorr(0),
    m_minCorr(0),
    m_medianFilterBuffer(0),
    m_sortedBuffer(0)
{
    m_decimationFactor = 8;
        
    // Chromagram configuration parameters
    ChromaConfig cconfig;
    cconfig.normalise = MathUtilities::NormaliseNone;
    cconfig.FS = config.sampleRate / (double)m_decimationFactor;
    if (cconfig.FS < 1) {
        cconfig.FS = 1;
    }

    // Set C3 (= MIDI #48) as our base:
    // This implies that key = 1 => Cmaj, key = 12 => Bmaj, key = 13 => Cmin, etc.
    cconfig.min = Pitch::getFrequencyForPitch(48, 0, config.tuningFrequency);
    // C7 (= MIDI #96) is the exclusive maximum key:
    cconfig.max = Pitch::getFrequencyForPitch(96, 0, config.tuningFrequency);

    m_BPO = 36;
    cconfig.BPO = m_BPO;
    cconfig.CQThresh = 0.0054;

    // Chromagram inst.
    m_chroma = new Chromagram(cconfig);

    // Get calculated parameters from chroma object
    m_chromaFrameSize = m_chroma->getFrameSize();
    m_chromaHopSize = m_chroma->getHopSize();

//    std::cerr << "chroma frame size = " << m_ChromaFrameSize << ", decimation factor = " << m_DecimationFactor << " therefore block size = " << getBlockSize() << std::endl;

    // Chromagram average and estimated key median filter lengths
    m_chromaBufferSize = 1; // (int)ceil( m_hpcpAverage * m_ChromaConfig.FS/m_ChromaFrameSize );
    m_medianWinSize = (int)ceil
        (m_medianAverage * cconfig.FS / m_chromaFrameSize);
    
    // Reset counters
    m_bufferIndex = 0;
    m_chromaBufferFilling = 0;
    m_medianBufferFilling = 0;

    // Spawn objects/arrays
    m_decimatedBuffer = new double[m_chromaFrameSize];
    m_chromaBuffer = new double[m_BPO * m_chromaBufferSize];

    memset(m_chromaBuffer, 0, sizeof(double) * m_BPO * m_chromaBufferSize);
    
    m_meanHPCP = new double[m_BPO];
    m_inTuneChroma = new double[m_BPO/3];
    
    m_majCorr = new double[m_BPO];
    m_minCorr = new double[m_BPO];
    
    m_medianFilterBuffer = new int[ m_medianWinSize ];
    memset(m_medianFilterBuffer, 0, sizeof(int) * m_medianWinSize);
    
    m_sortedBuffer = new int[ m_medianWinSize ];
    memset(m_sortedBuffer, 0, sizeof(int) * m_medianWinSize);
    
    m_decimator = new Decimator
        (m_chromaFrameSize * m_decimationFactor, m_decimationFactor);

    for (int k = 0; k < 25; k++) {
        m_progressionProbability[k] = 0;
    }

    for (int k = 0; k < 48; k++) {
        m_scaleProbability[k] = 0;
    }
}

KeyDetectorDaschuer::~KeyDetectorDaschuer()
{
    delete m_chroma;
    delete m_decimator;
    
    delete [] m_decimatedBuffer;
    delete [] m_chromaBuffer;
    delete [] m_meanHPCP;
    delete [] m_inTuneChroma;
    delete [] m_majCorr;
    delete [] m_minCorr;
    delete [] m_medianFilterBuffer;
    delete [] m_sortedBuffer;
}

int KeyDetectorDaschuer::process(double *pcmData)
{
    int j, k;

    m_decimator->process(pcmData, m_decimatedBuffer);

    m_chrPointer = m_chroma->process(m_decimatedBuffer);

    double maxNoteValue;
    MathUtilities::getMax(m_chrPointer, m_BPO, &maxNoteValue);

    // populate hpcp values;
    int cbidx;
    for (j = 0; j < m_BPO; j++) {
        cbidx = (m_bufferIndex * m_BPO) + j;
        m_chromaBuffer[ cbidx ] = m_chrPointer[j];
    }

    // keep track of input buffers;
    if (m_bufferIndex++ >= m_chromaBufferSize - 1) {
        m_bufferIndex = 0;
    }

    // track filling of chroma matrix
    if (m_chromaBufferFilling++ >= m_chromaBufferSize) {
        m_chromaBufferFilling = m_chromaBufferSize;
    }

    // calculate mean
    for (k = 0; k < m_BPO; k++) {
        double mnVal = 0.0;
        for (j = 0; j < m_chromaBufferFilling; j++) {
            mnVal += m_chromaBuffer[ k + (j * m_BPO) ];
        }
        
        m_meanHPCP[k] = mnVal / (double)m_chromaBufferFilling;
    }

    // Normalize for zero average
    double mHPCP = MathUtilities::mean(m_meanHPCP, m_BPO);
    for (k = 0; k < m_BPO; k++) {
        m_meanHPCP[k] -= mHPCP;
        m_meanHPCP[k] /= (maxNoteValue - mHPCP);
    }

#ifdef DEBUG_KEY_DETECTOR
    std::cout << " ";

    // Original Chroma
    for (int ii = 0; ii < m_BPO; ++ii) {
      double value = m_MeanHPCP[(ii+m_BPO-1) % m_BPO];
      if (value > 0 && maxNoteValue > 0.01) {
          if (value > 0.99) {
              std::cout << "Î";
          } else if (value > 0.66) {
              std::cout << "I";
          } else if (value > 0.33) {
              std::cout << "i";
          } else {
              std::cout << ";";
          }
      } else {
          if (ii == 3 || ii == 9  || ii == 18 ||  ii == 24 || ii == 30 ||
              ii == 4 || ii == 10 || ii == 19 ||  ii == 25 || ii == 31 ||
              ii == 5 || ii == 11 || ii == 20 ||  ii == 26 || ii == 32) {
              // Mark black keys
              std::cout << "-";
          }
          else {
              std::cout << "_";
          }
      }
      if (ii % 3 == 2) {
          std::cout << " ";
      }
    }
#endif

    double maxTunedValue = 0;

    // Use only notes in tune
    for (int ii = 0; ii < m_BPO / 3; ++ii) {
      int center = ii * 3;
      double value = m_meanHPCP[center];
      double flat = m_meanHPCP[(center+m_BPO-1) % m_BPO];
      double sharp = m_meanHPCP[center+1];
      if (value > 0.25 && maxNoteValue > 0.01
          && value > flat && value > sharp) {

#ifdef DEBUG_KEY_DETECTOR
          if (value > 0.99) {
              std::cout << "Î";
          } else if (value > 0.75) {
              std::cout << "I";
          } else if (value > 0.5) {
              std::cout << "i";
          } else {
              std::cout << ";";
          }
#endif
          
          m_inTuneChroma[ii] = value;
          if (maxTunedValue < value * maxNoteValue) {
              maxTunedValue = value * maxNoteValue;
          }
      } else {
#ifdef DEBUG_KEY_DETECTOR
          if (ii == 1 || ii == 3  || ii == 6 || ii == 8 || ii == 10) {
              // Mark black keys
              std::cout << "-";
          }
          else {
              std::cout << "_";
          }
#endif
          
          m_inTuneChroma[ii] = 0;
      }

#ifdef DEBUG_KEY_DETECTOR
      std::cout << " ";
#endif
    }

    const double smooth = 1.0 - 1.0/172.0; // For T of 16 s -> two frames at 120 BPM

    for (k = 0; k < 12; k++) {
        double sumMinor = 0;
        double sumMinorMelodic = 0;
        double sumMinorHarmonic = 0;
        double sumMinorGypsy = 0;

        for (int i = 0; i < 12; i++) {
            int j = (i - k + 12 + 3) % 12;

            sumMinor += m_inTuneChroma[i] * MinorScale[j];
            sumMinorMelodic += m_inTuneChroma[i] * MinorMelodicScale[j];
            sumMinorHarmonic += m_inTuneChroma[i] * MinorHarmonicScale[j];
            sumMinorGypsy += m_inTuneChroma[i] * MinorGypsyScale[j];
        }

        m_scaleProbability[k] = m_scaleProbability[k] * smooth + sumMinor * maxTunedValue;
        m_scaleProbability[k+12] = m_scaleProbability[k+12] * smooth + sumMinorMelodic * maxTunedValue;
        m_scaleProbability[k+24] = m_scaleProbability[k+24] * smooth + sumMinorHarmonic * maxTunedValue;
        m_scaleProbability[k+36] = m_scaleProbability[k+36] * smooth + sumMinorGypsy * maxTunedValue;

        m_maxTuneSum = m_maxTuneSum * smooth - maxTunedValue;
    }

    // Limit maximum unlikeness to be likely again within 16 s
    for (k = 0; k < 48; k++) {
        if (m_scaleProbability[k] < m_maxTuneSum / 2) {
            m_scaleProbability[k] = m_maxTuneSum / 2;
        }
    }

    double maxScaleValue;
    int scale = MathUtilities::getMax(m_scaleProbability, 48, &maxScaleValue) + 1;

    if (maxScaleValue < m_maxTuneSum / 6) {
        // 6 was adjusted from the undetectable outro of "Green Day" - "Boulevard of Broken Dreams"
        scale = 0;
    }

    double maxChordValue = 0;
    int maxChord = 0;

    for (k = 0; k < 12; k++) {
        
        m_majCorr[k] = 0;
        m_minCorr[k] = 0;

        for (int i = 0; i < 12; i++) {
            
            int j = (i - k + 12) % 12;

            m_majCorr[k] += m_inTuneChroma[i] * MajorChord[j];
            m_minCorr[k] += m_inTuneChroma[i] * MinorChord[j];
        }
        
        if (m_majCorr[k] > m_minCorr[k]) {
            if (maxChordValue < m_majCorr[k]) {
                maxChordValue = m_majCorr[k];
                maxChord = k + 1;
            }
        } else if (m_majCorr[k] < m_minCorr[k]) {
            if (maxChordValue < m_minCorr[k]) {
                maxChordValue = m_minCorr[k];
                maxChord = k + 1 + 12;
            }
        } else {
            // Single Note or no Min/Maj Chord
            if (maxChordValue < m_majCorr[k]) {
                maxChordValue = m_majCorr[k];
                maxChord = k + 1 + 24;
            }
        }
    }

    const double smoothing = 1.0 - 1.0/172.0; // For T of 16 s -> two frames at 120 BPM
    if (maxChord) {
        for (int k = 0; k < 12; k++) {
            if (maxChord <= 12) {
                m_progressionProbability[k+1] = m_progressionProbability[k+1] * smoothing +
                    maxTunedValue * (ChordToMajorKey[(maxChord-1-k+12) % 12]);
            } else if (maxChord <= 24) {
                m_progressionProbability[k+1] = m_progressionProbability[k+1] * smoothing +
                    maxTunedValue * (ChordToMajorKey[(maxChord-1-k+12) % 12 + 12]);
            } else {
                m_progressionProbability[k+1] = m_progressionProbability[k+1] * smoothing +
                    maxTunedValue * (NoteToKey[(maxChord-1-k+12) % 12 ]);
            }
        }

        for (int k = 12; k < 24; k++) {
            if (maxChord <= 12) {
                m_progressionProbability[k+1] = m_progressionProbability[k+1] * smoothing +
                    maxTunedValue * (ChordToMinorKey[(maxChord-1-k+24) % 12]);
            } else if (maxChord <= 24) {
                m_progressionProbability[k+1] = m_progressionProbability[k+1] * smoothing +
                    maxTunedValue * (ChordToMinorKey[(maxChord-1-k+24) % 12 + 12]);
            } else {
                m_progressionProbability[k+1] = m_progressionProbability[k+1] * smoothing +
                    maxTunedValue * (NoteToKey[(maxChord-1-k+24+9) % 12 ]);
            }
        }
        
        // unknown
        m_progressionProbability[0] = m_progressionProbability[0] * smoothing - maxTunedValue;
        
    } else {
        // unknown
        maxTunedValue = maxNoteValue;
        m_progressionProbability[0] = m_progressionProbability[0] * smoothing + maxTunedValue;
    }

    /*
    std::cout << "  ###";
    for( int k = 0; k < 25; k++ ) {
        m_ScaleProbability[1-1]
        std::cout << " " << m_ProgressionProbability[k];
    }
    std::cout << "  ###";
    */



    //for( int k = 0; k < 25; k++ )
    //{
    //    std::cout << "/" << m_KeyProbability[k];
    //}
    //d1 = (m_KeyProbability[1] - d1) / maxTunedValue;
    //d22 = (m_KeyProbability[22] - d22) / maxTunedValue;
    //std::cout << " " << d1 << " " << d22;
    //std::cout << " " << m_KeyProbability[4] << " " << m_KeyProbability[13];


    double dummy;
    int progression = MathUtilities::getMax(m_progressionProbability, 25, &dummy);

    int key = 0;
    if (scale > 0 && scale < 12 && progression > 0) {
        // we can only return western keys, sorry.
        double max = -100;
        for (int k = 0; k < 12; k++) {
            double value = m_scaleProbability[k];
            if (progression <= 12) {
                if (progression - 1 == k) {
                    // Consider the scale of the detected progression as more likely
                    // This means we allow more of scale notes if the progrssion still matches
                    value -= m_maxTuneSum / 60;  // m_maxTuneSum is negative
                }
            } else {
                if ((progression + 2) % 12 == k) {
                    // Consider the scale of the detected progression as more likely
                    // This means we allow more of scale notes if the progrssion still matches
                    value -= m_maxTuneSum / 60;  // m_maxTuneSum is negative
                }
            }
            if (value > max) {
                key = k + 1;
                max = value;
            }
        }
        int minorKey = ((key + 8) % 12) + 13;
        if (m_progressionProbability[key] < m_progressionProbability[minorKey]) {
            key = minorKey;
        }
    }

#ifdef DEBUG_KEY_DETECTOR
    std::cout << " " << maxChord << " " << progression << " " << scale << " " << key << " "
            << (m_processCall - 1) * m_ChromaHopSize / m_ChromaConfig.FS << std::endl;
#endif

    return key;
}

int
KeyDetectorDaschuer::getHopSize() const {
    return m_chromaHopSize * m_decimationFactor;
}

int
KeyDetectorDaschuer::getBlockSize() const {
    return m_chromaFrameSize * m_decimationFactor;
}

std::vector<double>
KeyDetectorDaschuer::getKeyStrengths() const {

    std::vector<double> keyStrengths;
    keyStrengths.resize(24, 0.0);

    for (int k = 0; k < m_BPO; k++) {
        int idx = k / (m_BPO/12);
        int rem = k % (m_BPO/12);
        if (rem == 0 || m_majCorr[k] > keyStrengths[idx]) {
            keyStrengths[idx] = m_majCorr[k];
        }
    }

    for (int k = 0; k < m_BPO; k++) {
        int idx = (k + m_BPO) / (m_BPO/12);
        int rem = k % (m_BPO/12);
        if (rem == 0 || m_minCorr[k] > keyStrengths[idx]) {
            keyStrengths[idx] = m_minCorr[k];
        }
    }

    return keyStrengths;
}

}
