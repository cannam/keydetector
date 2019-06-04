
#include <vamp/vamp.h>
#include <vamp-sdk/PluginAdapter.h>

#include "KeyDetectorPlugin.h"

static Vamp::PluginAdapter<KeyDetectorPlugin> keyDetectorPluginAdapter;

const VampPluginDescriptor *
vampGetPluginDescriptor(unsigned int version, unsigned int index)
{
    if (version < 1) return 0;

    switch (index) {
    case  0: return keyDetectorPluginAdapter.getDescriptor();
    default: return 0;
    }
}


