
#ifndef KEY_DETECTOR_PLUGIN_H
#define KEY_DETECTOR_PLUGIN_H

#include <vamp-sdk/Plugin.h>

#include "keydetector/KeyDetector.h"

using std::string;

class KeyDetectorPlugin : public Vamp::Plugin
{
public:
    KeyDetectorPlugin(float inputSampleRate);
    virtual ~KeyDetectorPlugin();

    string getIdentifier() const;
    string getName() const;
    string getDescription() const;
    string getMaker() const;
    int getPluginVersion() const;
    string getCopyright() const;

    InputDomain getInputDomain() const;
    size_t getPreferredBlockSize() const;
    size_t getPreferredStepSize() const;
    size_t getMinChannelCount() const;
    size_t getMaxChannelCount() const;

    ParameterList getParameterDescriptors() const;
    float getParameter(string identifier) const;
    void setParameter(string identifier, float value);

    ProgramList getPrograms() const;
    string getCurrentProgram() const;
    void selectProgram(string name);

    OutputList getOutputDescriptors() const;

    bool initialise(size_t channels, size_t stepSize, size_t blockSize);
    void reset();

    FeatureSet process(const float *const *inputBuffers,
                       Vamp::RealTime timestamp);

    FeatureSet getRemainingFeatures();

protected:
    float m_tuningFrequency;
    KD::KeyDetector::Method m_method;
    KD::KeyDetector *m_kd;
    mutable int m_stepSize;
    mutable int m_blockSize;
    std::vector<double> m_inputFrame;
    int m_prevKey;

    int getKeyIndexForCircleOf5thsIndex(int c5) const;
    string getKeyName(int index, bool minor, bool includeMajMin) const;
};


#endif
