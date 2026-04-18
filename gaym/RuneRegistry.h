#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "RuneDef.h"

// Singleton registry for all rune definitions.
// To add a new rune: add one Register({...}) call in RuneRegistry.cpp.
// To look up a rune anywhere: RuneRegistry::Get().Find("F02")
class RuneRegistry
{
public:
    static RuneRegistry& Get();

    // Look up a rune definition by ID (returns nullptr if not found)
    const RuneDef* Find(const std::string& id) const;

    // All registered rune definitions
    const std::unordered_map<std::string, RuneDef>& GetAll() const { return m_defs; }

    // All rune IDs of a specific grade (for drop generation)
    std::vector<std::string> GetIdsByGrade(RuneGrade grade) const;

private:
    RuneRegistry();  // builds the registry on first access
    void Register(RuneDef def);

    std::unordered_map<std::string, RuneDef> m_defs;
};
