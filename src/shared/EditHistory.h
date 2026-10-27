#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

// UI/model-thread history. Snapshots contain edits and parameters, never raw audio.
// The owner is the processor for ordinary VST/AU and the document for ARA, so closing
// an editor, Reload, and changing the selected source do not discard the history.
template <typename State>
class QQEditorHistory
{
public:
    bool record(const juce::String& key, const State& state, const juce::String& identity, bool dragging)
    {
        if (key.isEmpty()) return false;
        auto& history = sources[key];
        const bool hadState = ! history.entries.empty();
        if (! history.entries.empty() && history.entries[history.index].identity == identity)
        {
            history.dragging = dragging;
            return false;
        }
        if (! history.entries.empty() && history.dragging && dragging && history.index > 0
            && history.index + 1 == history.entries.size())
            history.entries.back() = { state, identity };
        else
        {
            if (! history.entries.empty()) history.entries.resize(history.index + 1);
            history.entries.push_back({ state, identity });
            if (history.entries.size() > 65) history.entries.erase(history.entries.begin());
            history.index = history.entries.size() - 1;
        }
        history.dragging = dragging;
        return hadState;
    }

    bool canMove(const juce::String& key, bool redo) const
    {
        const auto found = sources.find(key);
        if (found == sources.end() || found->second.entries.empty()) return false;
        const auto& h = found->second;
        return redo ? h.index + 1 < h.entries.size() : h.index > 0;
    }

    bool move(const juce::String& key, bool redo, State& state)
    {
        if (! canMove(key, redo)) return false;
        auto& h = sources.at(key);
        if (redo) ++h.index; else --h.index;
        h.dragging = false;
        state = h.entries[h.index].state;
        return true;
    }

    void endGesture(const juce::String& key)
    {
        const auto found = sources.find(key);
        if (found != sources.end()) found->second.dragging = false;
    }

    void erase(const juce::String& key) { sources.erase(key); }
    void clear() { sources.clear(); }

private:
    struct Entry { State state; juce::String identity; };
    struct History { std::vector<Entry> entries; size_t index = 0; bool dragging = false; };
    std::map<juce::String, History> sources;
};
