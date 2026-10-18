// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included after OscillatorDirect. Menu callbacks run in the SDK GUI context.
#include "CoreModules/context_menu.hh"
#include "filesystem/async_filebrowser.hh"
#include "patch/patch_file.hh"
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>
#include "dsp/table_encoding.hh"

template <surge_mm::Model Model> class TableOscillator : public OscillatorDirect<Model>
{
    using Base = OscillatorDirect<Model>;
    struct Packet
    {
        surge_mm::TableHandle table;
        Packet *next{};
    };
    // GUI owns pending submissions; audio owns active; GUI reclaims retired.
    // Engine handles cannot free the old buffers in update(): retired retains them.
    std::atomic<Packet *> pending{}, retired{};
    Packet *active{};
    surge_mm::TableHandle selected = surge_mm::default_wavetable();
    std::string name = "Sine To Sawtooth", status;
    static constexpr std::array<unsigned, 7> frame_sizes{0, 2048, 1024, 512, 4096, 256, 128};
    unsigned frame_choice{};
    struct Lifetime
    {
        TableOscillator *module;
    };
    std::shared_ptr<Lifetime> lifetime = std::make_shared<Lifetime>(Lifetime{this});
    void collect()
    {
        auto p = retired.exchange(nullptr);
        while (p)
        {
            auto next = p->next;
            delete p;
            p = next;
        }
    }
    void submit(surge_mm::TableHandle table, std::string label)
    {
        collect();
        auto next = new Packet{table};
        delete pending.exchange(next);
        selected = std::move(table);
        name = std::move(label);
        status.clear();
        this->invalidate_preview();
    }

  protected:
    void prepare_preview(surge_mm::Engine *preview) override
    {
        surge_mm::set_wavetable(preview, selected);
        collect();
    }

  public:
    ~TableOscillator() override
    {
        lifetime.reset();
        delete pending.exchange(nullptr);
        delete active;
        collect();
    }
    void update() override
    {
        if (auto next = pending.exchange(nullptr))
        {
            for (auto &voice : this->voices)
                surge_mm::set_wavetable(voice.get(), next->table);
            if (active)
            {
                auto head = retired.load();
                do
                {
                    active->next = head;
                } while (!retired.compare_exchange_weak(head, active));
            }
            active = next;
        }
        Base::update();
    }
    std::string save_state() override
    {
        collect();
        const auto &bytes = surge_mm::table_bytes(selected);
        std::string prefix = "SWT1:";
        if (!Base::save_state().empty())
            prefix = std::string("SWT2:") + char('0' + this->output_settings.halfband_order) +
                     char('0' + this->output_settings.steep) +
                     char('0' + this->output_settings.dc_block) +
                     char('0' + this->rack_input_routing) + ":";
        return prefix + std::to_string(frame_choice) + ":" +
               surge_mm::table_base64(reinterpret_cast<const uint8_t *>(name.data()), name.size()) +
               ":" + surge_mm::table_base64(bytes.data(), bytes.size());
    }
    void load_state(std::string_view state) override
    {
        if (state.empty())
        {
            Base::load_state("");
            auto defaults = surge_mm::create(Model);
            for (int i = 0; i < 10; ++i)
                this->set_param(i, surge_mm::get_param(defaults, i));
            for (int i = 0; i < (Model == surge_mm::Model::Window ? 6 : 4); ++i)
                this->set_param(10 + i, surge_mm::get_wavetable_option(defaults, i));
            this->set_param(surge_mm::table_extra_param(Model), .5f);
            this->set_param(surge_mm::table_extra_param(Model) + 1, .5f);
            surge_mm::destroy(defaults);
            frame_choice = 0;
            submit(surge_mm::default_wavetable(), "Sine To Sawtooth");
            return;
        }
        const bool extended = state.starts_with("SWT2:");
        const unsigned frame_at = extended ? 10 : 5;
        if (state.size() < frame_at + 3 || (!extended && !state.starts_with("SWT1:")) ||
            state[frame_at] < '0' || state[frame_at] > '6' || state[frame_at + 1] != ':')
            return;
        std::string output_state;
        if (extended)
        {
            if (state[5] < '1' || state[5] > '6' || state[6] < '0' || state[6] > '1' ||
                state[7] < '0' || state[7] > '1' || state[8] < '0' || state[8] > '1' ||
                state[9] != ':')
                return;
            output_state =
                std::string(state[8] == '1' ? "surge-osc-rack-v1 " : "surge-osc-output-v1 ") +
                state[5] + " " + state[6] + " " + state[7];
        }
        auto split = state.find(':', frame_at + 2);
        if (split == state.npos)
            return;
        std::vector<uint8_t> title, bytes;
        if (!surge_mm::table_unbase64(state.substr(frame_at + 2, split - frame_at - 2), title,
                                      1024) ||
            !surge_mm::table_unbase64(state.substr(split + 1), bytes, 12 + 4096 * 512 * 4))
            return;
        std::string error;
        auto table = surge_mm::load_table(bytes.data(), bytes.size(), error);
        if (!table)
            return;
        Base::load_state(output_state);
        frame_choice = state[frame_at] - '0';
        submit(std::move(table), std::string(title.begin(), title.end()));
    }
    bool load_file(const char *path)
    {
        collect();
        if (!path)
            return false;
        auto *file = std::fopen(path, "rb");
        if (!file)
        {
            status = "Cannot open table";
            return false;
        }
        if (std::fseek(file, 0, SEEK_END))
        {
            std::fclose(file);
            status = "Cannot read table";
            return false;
        }
        auto length = std::ftell(file);
        if (length < 12 || length > 64 * 1024 * 1024 || std::fseek(file, 0, SEEK_SET))
        {
            std::fclose(file);
            status = "Invalid table size";
            return false;
        }
        std::vector<uint8_t> bytes(static_cast<size_t>(length));
        auto read = std::fread(bytes.data(), 1, bytes.size(), file);
        std::fclose(file);
        if (read != bytes.size())
        {
            status = "Incomplete table read";
            return false;
        }
        std::string error;
        auto table =
            surge_mm::load_table(bytes.data(), bytes.size(), error, frame_sizes[frame_choice]);
        if (!table)
        {
            status = std::move(error);
            return false;
        }
        std::string label(path);
        auto slash = label.find_last_of("/\\");
        if (slash != label.npos)
            label.erase(0, slash + 1);
        submit(std::move(table), std::move(label));
        return true;
    }
    std::vector<MetaModule::ContextMenuItem> menu_items()
    {
        collect();
        using T = MetaModule::ContextMenuItem::Type;
        return {{.type = T::Label, .name = name},
                {.type = T::Action, .name = "Load wavetable..."},
                {.type = T::Action, .name = "Use Sine To Sawtooth"},
                {.type = T::Action,
                 .name = "WAV frame size",
                 .value_text = frame_choice ? std::to_string(frame_sizes[frame_choice]) : "Auto"},
                {.type = T::Label,
                 .name = status.empty() ? "WT / WAV; table stored in patch" : status}};
    }
    void menu_changed(unsigned index, float)
    {
        if (index == 1)
        {
            std::weak_ptr<Lifetime> token = lifetime;
            async_open_file("", ".wt, .WT, .wav, .WAV", "Load wavetable:", [token](char *path) {
                if (auto alive = token.lock(); alive && path)
                    if (alive->module->load_file(path))
                        MetaModule::Patch::mark_patch_modified();
                std::free(path);
            });
        }
        else if (index == 2)
        {
            submit(surge_mm::default_wavetable(), "Sine To Sawtooth");
            MetaModule::Patch::mark_patch_modified();
        }
        else if (index == 3)
        {
            frame_choice = (frame_choice + 1) % frame_sizes.size();
            MetaModule::Patch::mark_patch_modified();
        }
    }
};
