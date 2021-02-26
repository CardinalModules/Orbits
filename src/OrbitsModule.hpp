#pragma once

#include "plugin.hpp"
#include <vector>

struct EOCModeOption
{
        const char *desc;
        EOCModeOption(const char *_desc) : desc{_desc}
        {
        }

        virtual bool process(bool is_first, bool is_last, bool prev_was_last) const = 0;

        virtual ~EOCModeOption()
        {
        }
};

struct EOCModeOptionOnRepeat : EOCModeOption
{
        EOCModeOptionOnRepeat() : EOCModeOption("On Repeat")
        {
        }

        virtual ~EOCModeOptionOnRepeat() override
        {
        }

        bool process(bool is_first, bool is_last, bool prev_was_last) const override
        {
                return is_first && prev_was_last;
        }
};

struct EOCModeOptionFirst : EOCModeOption
{
        EOCModeOptionFirst() : EOCModeOption("First")
        {
        }

        virtual ~EOCModeOptionFirst() override
        {
        }

        bool process(bool is_first, bool is_last, bool prev_was_last) const override
        {
                return is_first;
        }
};

struct EOCModeOptionLast : EOCModeOption
{
        EOCModeOptionLast() : EOCModeOption("Last")
        {
        }

        bool process(bool is_first, bool is_last, bool prev_was_last) const override
        {
                return is_last;
        }
};

class EOCModeOptions
{
      public:
        bool process(int mode, bool is_first, bool is_last, bool prev_was_last) const;
        std::vector<std::string> getOptions(void) const;
        ~EOCModeOptions();
        size_t size() const;

      private:
        std::vector<EOCModeOption *> options{new EOCModeOptionOnRepeat, new EOCModeOptionFirst, new EOCModeOptionLast};
};

struct EOCMode
{
        int m_mode = 0;

        int getMode(void);
        void setMode(int mode);
        std::vector<std::string> getOptions(void);
        json_t *dataToJson(void);
        void dataFromJson(json_t *root);
};

struct EOCGenerator
{
        dsp::PulseGenerator m_generator;
        bool m_previous_beat_was_last = false;
        void update(EOCMode &mode, bool is_first, bool is_last);
        bool process(float delta);
};

struct RepeatTrigger
{
        const float m_repeat_delay_s = 0.5f;
        const float m_repeat_rate_s = 0.05f;

        dsp::BooleanTrigger m_trigger;
        dsp::PulseGenerator m_repeat_timer;

        bool process(bool state, float sampleTime)
        {
                if(m_trigger.process(state))
                {
                        m_repeat_timer.trigger(m_repeat_delay_s);
                        return true;
                }    

                if(state && !m_repeat_timer.process(sampleTime))
                {
                        m_repeat_timer.trigger(m_repeat_rate_s);
                        return true;
                }

                return false;
	    }
};
