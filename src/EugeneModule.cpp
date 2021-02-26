#include "EugeneModule.hpp"
#include "EugeneWidget.hpp"
#include "Rhythm.hpp"

RareBreeds_Orbits_Eugene::RareBreeds_Orbits_Eugene()
{
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LENGTH_KNOB_PARAM, 1.f, rhythm::max_length, rhythm::max_length, "Length");
        configParam(HITS_KNOB_PARAM, 0.f, 1.f, 0.5f, "Hits", "%", 0.f, 100.f);
        configParam(SHIFT_KNOB_PARAM, 0.f, rhythm::max_length - 1, 0.f, "Shift");
        configParam(LENGTH_CV_KNOB_PARAM, 0.f, 1.f, 0.f, "Length CV");
        configParam(HITS_CV_KNOB_PARAM, 0.f, 1.f, 0.f, "Hits CV");
        configParam(SHIFT_CV_KNOB_PARAM, 0.f, 1.f, 0.f, "Shift CV");
        configParam(REVERSE_KNOB_PARAM, 0.f, 1.f, 0.f, "Reverse");
        configParam(INVERT_KNOB_PARAM, 0.f, 1.f, 0.f, "Invert");

        // Ensure the default rhythm exists before drawing
        updateRhythm();
}

unsigned int RareBreeds_Orbits_Eugene::readLength()
{
        float value = params[LENGTH_KNOB_PARAM].getValue();
        if(inputs[LENGTH_CV_INPUT].isConnected())
        {
                // bipolar +-5V input
                float input = inputs[LENGTH_CV_INPUT].getVoltage();
                float normalized_input = input / 5.f;
                float attenuation = params[LENGTH_CV_KNOB_PARAM].getValue();
                value += attenuation * normalized_input * (rhythm::max_length - 1);
        }

        return clamp(int(std::round(value)), 1, rhythm::max_length);
}

unsigned int RareBreeds_Orbits_Eugene::readHits(unsigned int length)
{
        float value = params[HITS_KNOB_PARAM].getValue();
        if(inputs[HITS_CV_INPUT].isConnected())
        {
                // bipolar +-5V input
                float input = inputs[HITS_CV_INPUT].getVoltage();
                float normalized_input = input / 5.f;
                float attenuation = params[HITS_CV_KNOB_PARAM].getValue();
                value += attenuation * normalized_input;
        }

        float hits_float = value * length;
        return clamp(int(std::round(hits_float)), 0, length);
}

unsigned int RareBreeds_Orbits_Eugene::readShift(unsigned int length)
{
        float value = params[SHIFT_KNOB_PARAM].getValue();
        if(inputs[SHIFT_CV_INPUT].isConnected())
        {
                // bipolar +-5V input
                float input = inputs[SHIFT_CV_INPUT].getVoltage();
                float normalized_input = input / 5.f;
                float attenuation = params[SHIFT_CV_KNOB_PARAM].getValue();
                value += attenuation * normalized_input * (rhythm::max_length - 1);
        }

        return clamp(int(std::round(value)), 0, rhythm::max_length - 1) % length;
}

bool RareBreeds_Orbits_Eugene::readReverse()
{
        if(inputs[REVERSE_CV_INPUT].isConnected())
        {
                m_reverse_trigger.process(inputs[REVERSE_CV_INPUT].getVoltage());
                return m_reverse_trigger.isHigh();
        }
        else
        {
                return std::round(params[REVERSE_KNOB_PARAM].getValue());
        }
}

bool RareBreeds_Orbits_Eugene::readInvert()
{
        if(inputs[INVERT_CV_INPUT].isConnected())
        {
                m_invert_trigger.process(inputs[INVERT_CV_INPUT].getVoltage());
                return m_invert_trigger.isHigh();
        }
        else
        {
                return std::round(params[INVERT_KNOB_PARAM].getValue());
        }
}

void RareBreeds_Orbits_Eugene::updateOutput(const ProcessArgs &args)
{
        if(inputs[SYNC_INPUT].isConnected() && m_sync_trigger.process(inputs[SYNC_INPUT].getVoltage()))
        {
                m_current_step = 0;
        }

        if(inputs[CLOCK_INPUT].isConnected() && m_clock_trigger.process(inputs[CLOCK_INPUT].getVoltage()))
        {
                auto length = readLength();
                auto reverse = readReverse();

                m_eoc_generator.update(m_eoc, m_current_step == 0, m_current_step == (reverse ? 1 : length - 1));

                if(reverse)
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

                if(m_rhythm[m_current_step])
                {
                        m_output_generator.trigger(1e-3f);
                }

                if(!reverse)
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
        }

        outputs[BEAT_OUTPUT].setVoltage(m_output_generator.process(args.sampleTime) ? 10.f : 0.f);
        outputs[EOC_OUTPUT].setVoltage(m_eoc_generator.process(args.sampleTime) ? 10.f : 0.f);
}

void RareBreeds_Orbits_Eugene::updateEuclideanRhythm(uint32_t length, uint32_t hits, uint32_t shift, bool invert)
{
        m_rhythm = rhythm::rhythm(length, hits, shift);

        if(invert)
        {
                m_rhythm.flip();
        }
}

void RareBreeds_Orbits_Eugene::updateRhythm()
{
        bool update = false;

        unsigned int length = readLength();
        if(length != m_old_length)
        {
                m_old_length = length;
                update = true;

                // wrap the index to the new length
                // to avoid accessing the rhythm out of bounds
                if(m_current_step >= length)
                {
                        m_current_step = 0;
                }
        }

        unsigned int shift = readShift(length);
        if(shift != m_old_shift)
        {
                m_old_shift = shift;
                update = true;
        }

        unsigned int hits = readHits(length);
        if(hits != m_old_hits)
        {
                m_old_hits = hits;
                update = true;
        }

        bool reverse = readReverse();
        if(reverse != m_old_reverse)
        {
                m_old_reverse = reverse;
                update = true;
        }

        bool invert = readInvert();
        if(invert != m_old_invert)
        {
                m_old_invert = invert;
                update = true;
        }

        if(update)
        {
                updateEuclideanRhythm(length, hits, shift, invert);
        }
}

void RareBreeds_Orbits_Eugene::process(const ProcessArgs &args)
{
        updateRhythm();
        updateOutput(args);
}

json_t *RareBreeds_Orbits_Eugene::dataToJson()
{
        json_t *root = json_object();
        if(root)
        {
                json_object_set_new(root, "eoc", m_eoc.dataToJson());

                if(m_widget)
                {
                        json_t *w = m_widget->dataToJson();
                        if(w)
                        {
                                json_object_set_new(root, "widget", w);
                        }
                }
        }
        return root;
}

void RareBreeds_Orbits_Eugene::dataFromJson(json_t *root)
{
        if(root)
        {
                m_eoc.dataFromJson(json_object_get(root, "eoc"));

                if(m_widget)
                {
                        json_t *obj = json_object_get(root, "widget");
                        if(obj)
                        {
                                m_widget->dataFromJson(obj);
                        }
                }
        }
}

void RareBreeds_Orbits_Eugene::onReset()
{
        m_current_step = 0;
}
