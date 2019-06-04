
#include "KeyDetectorPlugin.h"

const KD::KeyDetector::Method DefaultMethod = KD::KeyDetector::METHOD_DASCHUER;

const float DefaultTuningFrequency = 440.f;

KeyDetectorPlugin::KeyDetectorPlugin(float inputSampleRate) :
    Plugin(inputSampleRate),
    m_tuningFrequency(DefaultTuningFrequency),
    m_method(DefaultMethod),
    m_kd(0),
    m_stepSize(0),
    m_blockSize(0),
    m_prevKey(-1)
{
}

KeyDetectorPlugin::~KeyDetectorPlugin()
{
    delete m_kd;
}

string
KeyDetectorPlugin::getIdentifier() const
{
    return "keydetector";
}

string
KeyDetectorPlugin::getName() const
{
    return "Key Detector";
}

string
KeyDetectorPlugin::getDescription() const
{
    // Return something helpful here!
    return "";
}

string
KeyDetectorPlugin::getMaker() const
{
    // Your name here
    return "";
}

int
KeyDetectorPlugin::getPluginVersion() const
{
    // Increment this each time you release a version that behaves
    // differently from the previous one
    return 1;
}

string
KeyDetectorPlugin::getCopyright() const
{
    // This function is not ideally named.  It does not necessarily
    // need to say who made the plugin -- getMaker does that -- but it
    // should indicate the terms under which it is distributed.  For
    // example, "Copyright (year). All Rights Reserved", or "GPL"
    return "";
}

KeyDetectorPlugin::InputDomain
KeyDetectorPlugin::getInputDomain() const
{
    return TimeDomain;
}

size_t
KeyDetectorPlugin::getPreferredBlockSize() const
{
    if (!m_blockSize) {
        KD::KeyDetector::Config config(m_method, m_inputSampleRate);
        KD::KeyDetector d(config);
        m_stepSize = d.getHopSize();
        m_blockSize = d.getBlockSize();
    }
    return m_blockSize;
}

size_t 
KeyDetectorPlugin::getPreferredStepSize() const
{
    if (!m_blockSize) {
        (void)getPreferredBlockSize(); // initialise them
    }
    return m_stepSize;
}

size_t
KeyDetectorPlugin::getMinChannelCount() const
{
    return 1;
}

size_t
KeyDetectorPlugin::getMaxChannelCount() const
{
    return 1;
}

KeyDetectorPlugin::ParameterList
KeyDetectorPlugin::getParameterDescriptors() const
{
    ParameterList list;

    ParameterDescriptor d;
    d.identifier = "method";
    d.name = "Detection Method";
    d.description = "Method to use for key estimation from chromagram";
    d.unit = "";
    d.minValue = (int) KD::KeyDetector::METHOD_QM;
    d.maxValue = (int) KD::KeyDetector::METHOD_DASCHUER;
    d.defaultValue = (int) DefaultMethod;
    d.isQuantized = true;
    d.quantizeStep = 1;
    d.valueNames.clear();
    d.valueNames.push_back("QM");
    d.valueNames.push_back("Daschuer");
    list.push_back(d);

    d.identifier = "tuning";
    d.name = "Tuning Frequency";
    d.description = "Frequency of concert A";
    d.unit = "Hz";
    d.minValue = 390;
    d.maxValue = 470;
    d.defaultValue = 440;
    d.isQuantized = false;
    d.valueNames.clear();
    list.push_back(d);

    return list;
}

float
KeyDetectorPlugin::getParameter(string identifier) const
{
    if (identifier == "method") {
        return (int) m_method;
    }
    if (identifier == "tuning") {
        return m_tuningFrequency;
    }
    return 0;
}

void
KeyDetectorPlugin::setParameter(string identifier, float value) 
{
    if (identifier == "method") {
        if (value < 0.5) {
            m_method = KD::KeyDetector::METHOD_QM;
        } else if (value < 1.5) {
            m_method = KD::KeyDetector::METHOD_DASCHUER;
        }
        m_stepSize = m_blockSize = 0; // require re-init
    }
    if (identifier == "tuning") {
        m_tuningFrequency = value;
    }
}

KeyDetectorPlugin::ProgramList
KeyDetectorPlugin::getPrograms() const
{
    ProgramList list;
    return list;
}

string
KeyDetectorPlugin::getCurrentProgram() const
{
    return ""; // no programs
}

void
KeyDetectorPlugin::selectProgram(string)
{
}

KeyDetectorPlugin::OutputList
KeyDetectorPlugin::getOutputDescriptors() const
{
    OutputList list;

    float outputRate = m_inputSampleRate / getPreferredStepSize();

    OutputDescriptor d;
    d.identifier = "tonic";
    d.name = "Tonic Pitch";
    d.unit = "";
    d.description = "Tonic of the estimated key (from C = 1 to B = 12, where 0 = no key detected)";
    d.hasFixedBinCount = true;
    d.binCount = 1;
    d.hasKnownExtents = true;
    d.isQuantized = true;
    d.minValue = 0;
    d.maxValue = 12;
    d.quantizeStep = 1;
    d.sampleRate = outputRate;
    d.sampleType = OutputDescriptor::VariableSampleRate;
    list.push_back(d);

    d.identifier = "mode";
    d.name = "Key Mode";
    d.unit = "";
    d.description = "Major or minor mode of the estimated key (major = 0, minor = 1)";
    d.hasFixedBinCount = true;
    d.binCount = 1;
    d.hasKnownExtents = true;
    d.isQuantized = true;
    d.minValue = 0;
    d.maxValue = 1;
    d.quantizeStep = 1;
    d.sampleRate = outputRate;
    d.sampleType = OutputDescriptor::VariableSampleRate;
    list.push_back(d);

    d.identifier = "key";
    d.name = "Key";
    d.unit = "";
    d.description = "Estimated key (from C major = 1 to B major = 12 and C minor = 13 to B minor = 24, where 0 = no key detected)";
    d.hasFixedBinCount = true;
    d.binCount = 1;
    d.hasKnownExtents = true;
    d.isQuantized = true;
    d.minValue = 1;
    d.maxValue = 24;
    d.quantizeStep = 1;
    d.sampleRate = outputRate;
    d.sampleType = OutputDescriptor::VariableSampleRate;
    list.push_back(d);

    d.identifier = "keystrength";
    d.name = "Key Strength Plot";
    d.unit = "";
    d.description = "Correlation of the chroma vector with stored key profile for each major and minor key";
    d.hasFixedBinCount = true;
    d.binCount = 25;
    d.hasKnownExtents = false;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::OneSamplePerStep;
    for (int i = 0; i < 24; ++i) {
        if (i == 12) d.binNames.push_back(" ");
        int idx = getKeyIndexForCircleOf5thsIndex(i);
        std::string label = getKeyName(idx > 12 ? idx-12 : idx, 
                                       i >= 12,
                                       true);
        d.binNames.push_back(label);
    }
    list.push_back(d);

    return list;
}

bool
KeyDetectorPlugin::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if (channels < getMinChannelCount() ||
	channels > getMaxChannelCount()) {
        return false;
    }

    KD::KeyDetector::Config config(m_method, m_inputSampleRate);
    config.tuningFrequency = m_tuningFrequency;
    m_kd = new KD::KeyDetector(config);

    m_stepSize = m_kd->getHopSize();
    m_blockSize = m_kd->getBlockSize();

    if (stepSize != size_t(m_stepSize) ||
        blockSize != size_t(m_blockSize)) {
        std::cerr << "KeyDetectorPlugin::initialise: ERROR: step/block sizes "
                  << stepSize << "/" << blockSize << " differ from required "
                  << m_stepSize << "/" << m_blockSize << std::endl;
        delete m_kd;
        m_kd = 0;
        return false;
    }

    m_inputFrame.resize(m_blockSize, 0.0);
    
    return true;
}

void
KeyDetectorPlugin::reset()
{
    delete m_kd;
    KD::KeyDetector::Config config(m_method, m_inputSampleRate);
    config.tuningFrequency = m_tuningFrequency;
    m_kd = new KD::KeyDetector(config);
    
    m_prevKey = -1;
}

int
KeyDetectorPlugin::getKeyIndexForCircleOf5thsIndex(int c5) const
{
    static int conversion[24] = {
        7, 12, 5, 10, 3, 8, 1, 6, 11, 4, 9, 2,
        16, 21, 14, 19, 24, 17, 22, 15, 20, 13, 18, 23
    };

    if (c5 < 0 || c5 > 23) {
        throw std::logic_error("Circle-of-5ths index out of range");
    }

    return conversion[c5];
}

string
KeyDetectorPlugin::getKeyName(int index, bool minor, bool includeMajMin) const
{
    // Keys are numbered with 1 => C, 12 => B
    // This is based on chromagram base set to a C in qm-dsp's GetKeyMode.cpp

    static const char *namesMajor[] = {
        "C", "Db", "D", "Eb",
        "E", "F", "F# / Gb", "G",
        "Ab", "A", "Bb", "B"
    };

    static const char *namesMinor[] = {
        "C", "C#", "D", "Eb / D#",
        "E", "F", "F#", "G",
        "G#", "A", "Bb", "B"
    };

    if (index < 1 || index > 12) {
        throw std::logic_error("Key index out of range");
    }

    string base;

    if (minor) base = namesMinor[index - 1];
    else base = namesMajor[index - 1];

    if (!includeMajMin) return base;

    if (minor) return base + " minor";
    else return base + " major";
}

KeyDetectorPlugin::FeatureSet
KeyDetectorPlugin::process(const float *const *inputBuffers,
                           Vamp::RealTime now)
{
    if (!m_kd) {
        return FeatureSet();
    }
        
    FeatureSet returnFeatures;

    for (int i = 0; i < m_kd->getBlockSize(); ++i) {
        m_inputFrame[i] = (double)inputBuffers[0][i];
    }

    int key = m_kd->process(m_inputFrame.data());
    bool minor = (key > 12);
    int tonic = key;
    if (tonic > 12) tonic -= 12;

    int prevTonic = m_prevKey;
    if (prevTonic > 12) prevTonic -= 12;

    bool prevMinor = (m_prevKey > 12);

    bool first = (m_prevKey < 0);

    if (first || (tonic != prevTonic)) {
        Feature feature;
        feature.hasTimestamp = true;
        feature.timestamp = now;
        feature.values.push_back((float)tonic);
        if (tonic == 0) {
            feature.label = "N";
        } else {
            feature.label = getKeyName(tonic, minor, false);
        }
        returnFeatures[0].push_back(feature); // tonic
    }

    if (first || (minor != prevMinor)) {
        if (tonic != 0) {
            Feature feature;
            feature.hasTimestamp = true;
            feature.timestamp = now;
            feature.values.push_back(minor ? 1.f : 0.f);
            feature.label = (minor ? "Minor" : "Major");
            returnFeatures[1].push_back(feature); // mode
        }
    }

    if (first || (key != m_prevKey)) {
        Feature feature;
        feature.hasTimestamp = true;
        feature.timestamp = now;
        feature.values.push_back((float)key);
        if (tonic == 0) {
            feature.label = "N";
        } else {
            feature.label = getKeyName(tonic, minor, true);
        }
        returnFeatures[2].push_back(feature); // key
    }

    m_prevKey = key;

    Feature ksf;
    ksf.values.reserve(25);
    std::vector<double> keystrengths = m_kd->getKeyStrengths();
    for (int i = 0; i < 24; ++i) {
        if (i == 12) ksf.values.push_back(-1);
        int idx = getKeyIndexForCircleOf5thsIndex(i);
        ksf.values.push_back(keystrengths[idx - 1]);
    }
    ksf.hasTimestamp = false;
    returnFeatures[3].push_back(ksf);

    return returnFeatures;
}

KeyDetectorPlugin::FeatureSet
KeyDetectorPlugin::getRemainingFeatures()
{
    return FeatureSet();
}

