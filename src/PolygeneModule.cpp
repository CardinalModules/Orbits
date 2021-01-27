#include "PolygeneModule.hpp"
#include "Euclidean.hpp"
#include "PolygeneWidget.hpp"

// TODO: Library function
static unsigned int clampRounded(float value, unsigned int min, unsigned int max)
{
        return clamp(int(std::round(value)), min, max);
}

static void json_load_real(json_t *root, const char *param, float *result)
{
        json_t *obj = json_object_get(root, param);
        if(obj)
        {
                *result = json_real_value(obj);
        }
}

static void json_load_bool(json_t *root, const char *param, bool *result)
{
        json_t *obj = json_object_get(root, param);
        if(obj)
        {
                *result = json_boolean_value(obj);
        }
}

static void json_load_integer(json_t *root, const char *param, int *result)
{
        json_t *obj = json_object_get(root, param);
        if(obj)
        {
                *result = json_integer_value(obj);
        }
}

void RareBreeds_Orbits_Polygene::Channel::init(RareBreeds_Orbits_Polygene *module, int channel)
{
        m_module = module;
        m_channel = channel;
        m_length = m_module->params[LENGTH_KNOB_PARAM].getValue();
        m_length_cv = m_module->params[LENGTH_CV_KNOB_PARAM].getValue();
        m_hits = m_module->params[HITS_KNOB_PARAM].getValue();
        m_hits_cv = m_module->params[HITS_CV_KNOB_PARAM].getValue();
        m_shift = m_module->params[SHIFT_KNOB_PARAM].getValue();
        m_shift_cv = m_module->params[SHIFT_CV_KNOB_PARAM].getValue();
        m_oddity = m_module->params[ODDITY_KNOB_PARAM].getValue();
        m_oddity_cv = m_module->params[ODDITY_CV_KNOB_PARAM].getValue();
        m_reverse = false;
        m_invert = false;
}

void RareBreeds_Orbits_Polygene::Channel::toggleReverse(void)
{
        m_reverse = !m_reverse;
}

bool RareBreeds_Orbits_Polygene::Channel::readReverse(void)
{
        if(m_module->inputs[REVERSE_CV_INPUT].isConnected())
        {
                m_reverse_trigger.process(m_module->inputs[REVERSE_CV_INPUT].getPolyVoltage(m_channel));
                return m_reverse_trigger.isHigh() != m_reverse;
        }
        else
        {
                return m_reverse;
        }
}

void RareBreeds_Orbits_Polygene::Channel::toggleInvert(void)
{
        m_invert = !m_invert;
}

bool RareBreeds_Orbits_Polygene::Channel::readInvert(void)
{
        if(m_module->inputs[INVERT_CV_INPUT].isConnected())
        {
                m_invert_trigger.process(m_module->inputs[INVERT_CV_INPUT].getPolyVoltage(m_channel));
                return m_invert_trigger.isHigh() != m_invert;
        }
        else
        {
                return m_invert;
        }
}

bool RareBreeds_Orbits_Polygene::Channel::isOnBeat(unsigned int length, unsigned int hits, unsigned int shift,
                                                   unsigned int oddity, unsigned int beat, bool invert)
{
        return euclidean::nearEvenRhythmBeat(length, hits, oddity, shift, beat) != invert;
}

unsigned int RareBreeds_Orbits_Polygene::Channel::readLength()
{
        auto cv = m_module->inputs[LENGTH_CV_INPUT].getNormalPolyVoltage(0.0f, m_channel) / 5.f;
        auto f_length = m_length + m_length_cv * cv * (euclidean::max_length - 1);
        return clampRounded(f_length, 1, euclidean::max_length);
}

unsigned int RareBreeds_Orbits_Polygene::Channel::readHits(unsigned int length)
{
        auto cv = m_module->inputs[HITS_CV_INPUT].getNormalPolyVoltage(0.0f, m_channel) / 5.f;
        auto f_hits = m_hits + m_hits_cv * cv;
        return clampRounded(f_hits * length, 0, length);
}

unsigned int RareBreeds_Orbits_Polygene::Channel::readShift(unsigned int length)
{
        auto cv = m_module->inputs[SHIFT_CV_INPUT].getNormalPolyVoltage(0.0f, m_channel) / 5.f;
        auto f_shift = m_shift + m_shift_cv * cv * (euclidean::max_length - 1);
        return clampRounded(f_shift, 0, euclidean::max_length - 1) % length;
}

unsigned int RareBreeds_Orbits_Polygene::Channel::readOddity(unsigned int length, unsigned int hits)
{
        auto cv = m_module->inputs[ODDITY_CV_INPUT].getNormalPolyVoltage(0.0f, m_channel) / 5.f;
        auto f_oddity = m_oddity + m_oddity_cv * cv;
        auto count = euclidean::numNearEvenRhythms(length, hits);
        return clampRounded(f_oddity * (count - 1), 0, count - 1);
}

void RareBreeds_Orbits_Polygene::Channel::process(const ProcessArgs &args)
{
        auto length = readLength();

        // Avoid stepping out of bounds
        if(m_current_step >= length)
        {
                m_current_step = 0;
        }

        if(m_module->inputs[SYNC_INPUT].getChannels() > m_channel)
        {
                m_sync_trigger.process(m_module->inputs[SYNC_INPUT].getPolyVoltage(m_channel));
                if(m_sync_trigger.isHigh())
                {
                        m_apply_sync = true;
                }
        }

        if(m_module->inputs[CLOCK_INPUT].getChannels() > m_channel &&
           m_clock_trigger.process(m_module->inputs[CLOCK_INPUT].getPolyVoltage(m_channel)))
        {
                if(readReverse())
                {
                        if(m_current_step == 0)
                        {
                                m_current_step = length - 1;
                        }
                        else
                        {
                                --m_current_step;
                        }
                }
                else
                {
                        if(m_current_step == length - 1)
                        {
                                m_current_step = 0;
                        }
                        else
                        {
                                ++m_current_step;
                        }
                }

                if(m_apply_sync)
                {
                        m_apply_sync = false;
                        m_current_step = 0;
                }

                auto hits = readHits(length);
                auto shift = readShift(length);
                auto invert = readInvert();
                auto oddity = readOddity(length, hits);
                if(isOnBeat(length, hits, shift, oddity, m_current_step, invert))
                {
                        m_output_generator.trigger(1e-3f);
                }
        }

        auto out = m_output_generator.process(args.sampleTime) ? 10.f : 0.f;
        m_module->outputs[BEAT_OUTPUT].setVoltage(out, m_channel);
}

json_t *RareBreeds_Orbits_Polygene::Channel::dataToJson()
{
        json_t *root = json_object();
        if(root)
        {
                json_object_set_new(root, "length", json_real(m_length));
                json_object_set_new(root, "length_cv", json_real(m_length_cv));
                json_object_set_new(root, "hits", json_real(m_hits));
                json_object_set_new(root, "hits_cv", json_real(m_hits_cv));
                json_object_set_new(root, "shift", json_real(m_shift));
                json_object_set_new(root, "shift_cv", json_real(m_shift_cv));
                json_object_set_new(root, "oddity", json_real(m_oddity));
                json_object_set_new(root, "oddity_cv", json_real(m_oddity_cv));
                json_object_set_new(root, "reverse", json_boolean(m_reverse));
                json_object_set_new(root, "invert", json_boolean(m_invert));
        }
        return root;
}

void RareBreeds_Orbits_Polygene::Channel::dataFromJson(json_t *root)
{
        if(root)
        {
                json_load_real(root, "length", &m_length);
                json_load_real(root, "length_cv", &m_length_cv);
                json_load_real(root, "hits", &m_hits);
                json_load_real(root, "hits_cv", &m_hits_cv);
                json_load_real(root, "shift", &m_shift);
                json_load_real(root, "shift_cv", &m_shift_cv);
                json_load_real(root, "oddity", &m_oddity);
                json_load_real(root, "oddity_cv", &m_oddity_cv);
                json_load_bool(root, "reverse", &m_reverse);
                json_load_bool(root, "invert", &m_invert);
        }
}

void RareBreeds_Orbits_Polygene::Channel::onRandomize()
{
        m_length = random::uniform() * (euclidean::max_length - 1) + 1;
        m_length_cv = random::uniform();
        m_hits = random::uniform();
        m_hits_cv = random::uniform();
        m_shift = random::uniform();
        m_shift_cv = random::uniform();
        m_oddity = random::uniform();
        m_oddity_cv = random::uniform();
        m_reverse = (random::uniform() < 0.5f);
        m_invert = (random::uniform() < 0.5f);
}

RareBreeds_Orbits_Polygene::RareBreeds_Orbits_Polygene()
{
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(CHANNEL_KNOB_PARAM, 0.f, 15.f, 0.f, "Channel", "", 0.f, 1.f, 1.f);
        configParam(LENGTH_KNOB_PARAM, 1.f, euclidean::max_length, euclidean::max_length, "Length");
        configParam(HITS_KNOB_PARAM, 0.f, 1.f, 0.5f, "Hits", "%", 0.f, 100.f);
        configParam(SHIFT_KNOB_PARAM, 0.f, euclidean::max_length - 1, 0.f, "Shift");
        configParam(ODDITY_KNOB_PARAM, 0.f, 1.f, 0.0f, "Oddity", "%", 0.f, 100.f);
        configParam(LENGTH_CV_KNOB_PARAM, 0.f, 1.f, 0.f, "Length CV");
        configParam(HITS_CV_KNOB_PARAM, 0.f, 1.f, 0.f, "Hits CV");
        configParam(SHIFT_CV_KNOB_PARAM, 0.f, 1.f, 0.f, "Shift CV");
        configParam(ODDITY_CV_KNOB_PARAM, 0.f, 1.f, 0.f, "Oddity CV");
        configParam(REVERSE_KNOB_PARAM, 0.f, 1.f, 0.f, "Reverse");
        configParam(INVERT_KNOB_PARAM, 0.f, 1.f, 0.f, "Invert");

        reset();
}

void RareBreeds_Orbits_Polygene::reset()
{
        m_active_channel_id = 0;
        m_active_channel = &m_channels[m_active_channel_id];

        m_length = params[LENGTH_KNOB_PARAM].getValue();
        m_length_cv = params[LENGTH_CV_KNOB_PARAM].getValue();
        m_hits = params[HITS_KNOB_PARAM].getValue();
        m_hits_cv = params[HITS_CV_KNOB_PARAM].getValue();
        m_shift = params[SHIFT_KNOB_PARAM].getValue();
        m_shift_cv = params[SHIFT_CV_KNOB_PARAM].getValue();
        m_oddity = params[ODDITY_KNOB_PARAM].getValue();
        m_oddity_cv = params[ODDITY_CV_KNOB_PARAM].getValue();

        for(auto i = 0u; i < max_channels; ++i)
        {
                m_channels[i].init(this, i);
        }
}

void RareBreeds_Orbits_Polygene::process(const ProcessArgs &args)
{
        auto active_channels = inputs[CLOCK_INPUT].getChannels();
        outputs[BEAT_OUTPUT].setChannels(active_channels);

        m_active_channel_id = std::round(params[CHANNEL_KNOB_PARAM].getValue());
        m_active_channel = &m_channels[m_active_channel_id];

        float length = params[LENGTH_KNOB_PARAM].getValue();
        if(length != m_length)
        {
                m_active_channel->m_length = length;
                m_length = length;
        }

        float length_cv = params[LENGTH_CV_KNOB_PARAM].getValue();
        if(length_cv != m_length_cv)
        {
                m_active_channel->m_length_cv = length_cv;
                m_length_cv = length_cv;
        }

        float hits = params[HITS_KNOB_PARAM].getValue();
        if(hits != m_hits)
        {
                m_active_channel->m_hits = hits;
                m_hits = hits;
        }

        float hits_cv = params[HITS_CV_KNOB_PARAM].getValue();
        if(hits_cv != m_hits_cv)
        {
                m_active_channel->m_hits_cv = hits_cv;
                m_hits_cv = hits_cv;
        }

        float shift = params[SHIFT_KNOB_PARAM].getValue();
        if(shift != m_shift)
        {
                m_active_channel->m_shift = shift;
                m_shift = shift;
        }

        float shift_cv = params[SHIFT_CV_KNOB_PARAM].getValue();
        if(shift_cv != m_shift_cv)
        {
                m_active_channel->m_shift_cv = shift_cv;
                m_shift_cv = shift_cv;
        }

        float oddity = params[ODDITY_KNOB_PARAM].getValue();
        if(oddity != m_oddity)
        {
                m_active_channel->m_oddity = oddity;
                m_oddity = oddity;
        }

        float oddity_cv = params[ODDITY_CV_KNOB_PARAM].getValue();
        if(oddity_cv != m_oddity_cv)
        {
                m_active_channel->m_oddity_cv = oddity_cv;
                m_oddity_cv = oddity_cv;
        }

        if(reverse_trigger.process(std::round(params[REVERSE_KNOB_PARAM].getValue())))
        {
                m_active_channel->toggleReverse();
        }

        if(invert_trigger.process(std::round(params[INVERT_KNOB_PARAM].getValue())))
        {
                m_active_channel->toggleInvert();
        }

        for(auto i = 0u; i < max_channels; ++i)
        {
                m_channels[i].process(args);
        }
}

json_t *RareBreeds_Orbits_Polygene::dataToJson()
{
        json_t *root = json_object();
        if(root)
        {
                json_object_set_new(root, "length", json_real(m_length));
                json_object_set_new(root, "length_cv", json_real(m_length_cv));
                json_object_set_new(root, "hits", json_real(m_hits));
                json_object_set_new(root, "hits_cv", json_real(m_hits_cv));
                json_object_set_new(root, "shift", json_real(m_shift));
                json_object_set_new(root, "shift_cv", json_real(m_shift_cv));
                json_object_set_new(root, "oddity", json_real(m_oddity));
                json_object_set_new(root, "oddity_cv", json_real(m_oddity_cv));
                json_object_set_new(root, "active_channel_id", json_integer(m_active_channel_id));

                json_t *channels = json_array();
                if(channels)
                {
                        for(auto i = 0u; i < max_channels; ++i)
                        {
                                json_t *channel_json = m_channels[i].dataToJson();
                                if(channel_json)
                                {
                                        json_array_append_new(channels, channel_json);
                                }
                        }

                        json_object_set_new(root, "channels", channels);
                }

                if(widget)
                {
                        json_t *w = widget->dataToJson();
                        if(w)
                        {
                                json_object_set_new(root, "widget", w);
                        }
                }
        }

        return root;
}

void RareBreeds_Orbits_Polygene::dataFromJson(json_t *root)
{
        if(root)
        {
                json_load_real(root, "length", &m_length);
                json_load_real(root, "length_cv", &m_length_cv);
                json_load_real(root, "hits", &m_hits);
                json_load_real(root, "hits_cv", &m_hits_cv);
                json_load_real(root, "shift", &m_shift);
                json_load_real(root, "shift_cv", &m_shift_cv);
                json_load_real(root, "oddity", &m_oddity);
                json_load_real(root, "oddity_cv", &m_oddity_cv);
                json_load_integer(root, "active_channel_id", &m_active_channel_id);
                json_t *channels = json_object_get(root, "channels");
                if(channels)
                {
                        for(auto i = 0u; i < max_channels; ++i)
                        {
                                json_t *channel = json_array_get(channels, i);
                                if(channel)
                                {
                                        m_channels[i].dataFromJson(channel);
                                }
                        }
                }

                if(widget)
                {
                        json_t *obj = json_object_get(root, "widget");
                        if(obj)
                        {
                                widget->dataFromJson(obj);
                        }
                }
        }
}

void RareBreeds_Orbits_Polygene::onRandomize()
{
        for(auto i = 0u; i < max_channels; ++i)
        {
                m_channels[i].onRandomize();
        }

        // Parameters have already been randomised by VCV rack
        // But then the active channel controlled by those parameters has been randomised again
        // Update the parameters so they reflect the active channels randomised parameters
        params[LENGTH_KNOB_PARAM].setValue(m_active_channel->m_length);
        params[LENGTH_CV_KNOB_PARAM].setValue(m_active_channel->m_length_cv);
        params[HITS_KNOB_PARAM].setValue(m_active_channel->m_hits);
        params[HITS_CV_KNOB_PARAM].setValue(m_active_channel->m_hits_cv);
        params[SHIFT_KNOB_PARAM].setValue(m_active_channel->m_shift);
        params[SHIFT_CV_KNOB_PARAM].setValue(m_active_channel->m_shift_cv);
        params[ODDITY_KNOB_PARAM].setValue(m_active_channel->m_oddity);
        params[ODDITY_CV_KNOB_PARAM].setValue(m_active_channel->m_oddity_cv);
}

void RareBreeds_Orbits_Polygene::onReset()
{
        reset();
}
