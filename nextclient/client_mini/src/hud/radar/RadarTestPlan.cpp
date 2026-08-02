#include "RadarTestPlan.h"
#include "RadarDump.h"
#include "RadarShot.h"
#include "RadarFade.h"
#include "RadarBroadcast.h"
#include "RadarUtil.h"
#include "SmokeRegistry.h"
#include "../../main.h"

#include <demo_api.h>
#include <screenfade.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    enum class Trigger
    {
        Time,
        Every,
        Event
    };
    enum class Event
    {
        Flash,
        Smoke,
        Bomb,
        Hostage
    };

    struct Step
    {
        Trigger trigger = Trigger::Time;
        bool dump = false;
        bool shot = false;
        std::string base;

        float time = 0.0f; // Time: fire time; Every: interval (demo seconds)
        int count = 1; // Every: number of captures
        Event event = Event::Flash;
        float delay = 0.0f; // Event: capture this long after the rising edge

        int fired = 0; // Every: captures taken so far
        bool waiting = false; // Event: edge seen, waiting out the delay
        float fire_at = 0.0f; // Event: demo time to fire at
        bool done = false;
    };

    std::vector<Step> g_Steps;
    bool g_Armed;
    bool g_BaselineSet;
    bool g_WasPlaying;
    bool g_DoneWritten;
    bool g_QuitAfter;
    float g_Baseline;
    int g_Settle;
    std::string g_Marker = "radar_testplan.done";

    // Rising-edge state for event triggers, seeded on the first playback frame.
    bool g_EventInit;
    int g_PrevSmoke;
    int g_PrevHost;
    bool g_PrevBomb;
    bool g_PrevFlash;

    float DefaultDelay(Event e)
    {
        switch (e)
        {
            case Event::Flash:
                return 0.10f; // near the peak of the wash
            case Event::Smoke:
                return 1.20f; // let the cloud grow to full size
            case Event::Bomb:
                return 0.30f;
            case Event::Hostage:
                return 0.0f;
        }
        return 0.0f;
    }

    bool ParseEvent(const std::string& s, Event& out)
    {
        if (s == "flash")
        {
            out = Event::Flash;
        }
        else if (s == "smoke")
        {
            out = Event::Smoke;
        }
        else if (s == "bomb")
        {
            out = Event::Bomb;
        }
        else if (s == "hostage")
        {
            out = Event::Hostage;
        }
        else
        {
            return false;
        }
        return true;
    }

    bool ParseFloat(const std::string& s, float& out)
    {
        char* end = nullptr;
        out = std::strtof(s.c_str(), &end);
        return end != s.c_str() && *end == '\0';
    }

    std::filesystem::path GameDirPath(const std::string& name)
    {
        return RadarUtil_AnchorPath(gEngfuncs.pfnGetGameDirectory(), name.c_str());
    }

    bool FlashActive(double now)
    {
        screenfade_t fade;
        std::memset(&fade, 0, sizeof(fade));
        gEngfuncs.pfnGetScreenFade(&fade);
        if (!RadarFade::NearWhite(fade))
        {
            return false;
        }
        return RadarFade::Alpha(fade, now) > 40;
    }

    bool ParseStep(const std::string& first, std::istringstream& is, Step& s)
    {
        std::string action;
        if (first == "every")
        {
            int cnt = 0;
            if (!(is >> s.time >> cnt >> action >> s.base))
            {
                return false;
            }
            s.trigger = Trigger::Every;
            s.count = cnt > 0 ? cnt : 1;
        }
        else if (first == "on")
        {
            std::string ev;
            if (!(is >> ev) || !ParseEvent(ev, s.event))
            {
                return false;
            }
            std::string next;
            if (!(is >> next))
            {
                return false;
            }
            float d = 0.0f;
            if (ParseFloat(next, d))
            {
                s.delay = d;
                if (!(is >> action))
                {
                    return false;
                }
            }
            else
            {
                s.delay = DefaultDelay(s.event);
                action = next;
            }
            if (!(is >> s.base))
            {
                return false;
            }
            s.trigger = Trigger::Event;
        }
        else
        {
            std::string time_tok = first;
            if (first == "at" && !(is >> time_tok))
            {
                return false;
            }
            if (!ParseFloat(time_tok, s.time))
            {
                return false;
            }
            if (!(is >> action >> s.base))
            {
                return false;
            }
            s.trigger = Trigger::Time;
        }
        s.dump = action == "dump" || action == "both";
        s.shot = action == "shot" || action == "both";
        return (s.dump || s.shot) && !s.base.empty();
    }

    void WriteMarker(bool complete)
    {
        auto done = std::ranges::count_if(g_Steps, &Step::done);
        std::ofstream f(GameDirPath(g_Marker), std::ios::binary);
        if (f)
        {
            f << (complete ? "complete" : "incomplete") << " " << done << "/" << g_Steps.size() << "\n";
        }
    }

    bool LoadPlan(const std::filesystem::path& path, bool consume)
    {
        std::ifstream f(path);
        if (!f)
        {
            return false;
        }
        g_Steps.clear();
        g_Marker = "radar_testplan.done";
        g_QuitAfter = false;

        std::string line;
        while (std::getline(f, line))
        {
            size_t hash = line.find('#');
            if (hash != std::string::npos)
            {
                line.resize(hash);
            }
            std::istringstream is(line);
            std::string tok;
            if (!(is >> tok))
            {
                continue;
            }
            if (tok == "quit_after_last")
            {
                int v = 0;
                is >> v;
                g_QuitAfter = v != 0;
                continue;
            }
            if (tok == "done")
            {
                std::string m;
                if (is >> m)
                {
                    g_Marker = m;
                }
                continue;
            }
            Step s;
            if (ParseStep(tok, is, s))
            {
                g_Steps.push_back(s);
            }
        }
        f.close();

        if (consume)
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
        if (g_Steps.empty())
        {
            return false;
        }

        g_Armed = true;
        g_BaselineSet = false;
        g_WasPlaying = false;
        g_DoneWritten = false;
        g_EventInit = false;
        g_Settle = 0;
        gEngfuncs.Con_Printf("radar_testplan: armed %d step(s)\n", (int)g_Steps.size());
        return true;
    }

    void RadarTestPlanCmd()
    {
        const char* arg = gEngfuncs.Cmd_Argc() > 1 ? gEngfuncs.Cmd_Argv(1) : nullptr;
        if (!arg || !arg[0])
        {
            gEngfuncs.Con_Printf("usage: radar_testplan <file>\n");
            return;
        }
        if (!LoadPlan(GameDirPath(arg), false))
        {
            gEngfuncs.Con_Printf("radar_testplan: failed to load %s\n", arg);
        }
    }

    bool AllDone()
    {
        return std::ranges::all_of(g_Steps, &Step::done);
    }

    void FireStep(Step& s, const std::string& name)
    {
        if (s.dump)
        {
            RadarDump::Request((name + ".json").c_str());
        }
        if (s.shot)
        {
            RadarShot::Request((name + ".tga").c_str());
        }
        gEngfuncs.Con_Printf("radar_testplan: capture -> %s\n", name.c_str());
    }
} // namespace

namespace RadarTestPlan
{
    void Init()
    {
        gEngfuncs.pfnAddCommand(const_cast<char*>("radar_testplan"), RadarTestPlanCmd);

        std::error_code ec;
        std::filesystem::path p = GameDirPath("radar_testplan.txt");
        if (std::filesystem::exists(p, ec))
        {
            LoadPlan(p, true);
        }
    }

    void Tick()
    {
        if (!g_Armed)
        {
            return;
        }

        if (AllDone())
        {
            if (g_Settle > 0)
            {
                g_Settle--;
                return;
            }
            if (!g_DoneWritten)
            {
                g_DoneWritten = true;
                WriteMarker(true);
                gEngfuncs.Con_Printf("radar_testplan: done\n");
                if (g_QuitAfter)
                {
                    gEngfuncs.pfnClientCmd("quit\n");
                }
                g_Armed = false;
            }
            return;
        }

        demo_api_t* demo = gEngfuncs.pDemoAPI;
        bool playing = demo != nullptr && demo->IsPlayingback() != 0;
        if (!playing)
        {
            // Playback stopped after the plan had started: finish early so the harness is
            // not left waiting for steps the demo will never reach.
            if (g_BaselineSet && g_WasPlaying && !g_DoneWritten)
            {
                g_DoneWritten = true;
                WriteMarker(false);
                gEngfuncs.Con_Printf("radar_testplan: demo ended before plan completed\n");
                g_Armed = false;
            }
            return;
        }
        g_WasPlaying = true;

        double now = gEngfuncs.GetClientTime();
        if (!g_BaselineSet)
        {
            g_BaselineSet = true;
            g_Baseline = (float)now;
        }
        float elapsed = (float)now - g_Baseline;

        RadarSmoke::Disc discs[64];
        RadarBroadcast::Hostage hosts[32];
        int smoke = RadarSmoke::Collect(discs, 64, (float)now);
        int host = RadarBroadcast::CollectHostages(hosts, 32);
        bool bomb = RadarBroadcast::GetBomb().planted;
        bool flash = FlashActive(now);
        if (!g_EventInit)
        {
            g_EventInit = true;
            g_PrevSmoke = smoke;
            g_PrevHost = host;
            g_PrevBomb = bomb;
            g_PrevFlash = flash;
        }
        bool smoke_edge = smoke > g_PrevSmoke;
        bool host_edge = host > g_PrevHost;
        bool bomb_edge = bomb && !g_PrevBomb;
        bool flash_edge = flash && !g_PrevFlash;
        g_PrevSmoke = smoke;
        g_PrevHost = host;
        g_PrevBomb = bomb;
        g_PrevFlash = flash;

        // At most one step is fired per frame: Request overwrites a single pending slot,
        // so two shots in one frame would clobber each other. Steps spread over frames.
        for (Step& s : g_Steps)
        {
            if (s.done)
            {
                continue;
            }
            bool fire = false;
            std::string name = s.base;
            if (s.trigger == Trigger::Time)
            {
                fire = elapsed >= s.time;
            }
            else if (s.trigger == Trigger::Every)
            {
                if (elapsed >= s.time * (float)(s.fired + 1))
                {
                    fire = true;
                    name = s.base + "_" + std::to_string(s.fired);
                }
            }
            else
            {
                if (!s.waiting)
                {
                    bool edge = (s.event == Event::Flash && flash_edge) || (s.event == Event::Smoke && smoke_edge) ||
                                (s.event == Event::Bomb && bomb_edge) || (s.event == Event::Hostage && host_edge);
                    if (edge)
                    {
                        s.waiting = true;
                        s.fire_at = (float)now + s.delay;
                    }
                }
                fire = s.waiting && (float)now >= s.fire_at;
            }
            if (fire)
            {
                FireStep(s, name);
                if (s.trigger == Trigger::Every)
                {
                    s.fired++;
                    s.done = s.fired >= s.count;
                }
                else
                {
                    s.done = true;
                }
                break;
            }
        }

        if (AllDone())
        {
            g_Settle = 4; // let the queued captures flush before the done marker
        }
    }
} // namespace RadarTestPlan
