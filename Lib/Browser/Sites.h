#pragma once

#include <Miro/Reflect.h>
#include <emberstore/Emberstore.h>

#include <map>
#include <string>
#include <vector>

namespace grazer
{
struct SiteSettings
{
    // Grazer single-key bindings the site keeps for its own shortcuts. A key
    // also drops every sequence it starts ("g" drops "gg"); "all" drops
    // them all.
    std::vector<std::string> excludeKeybindings;

    MIRO_REFLECT(excludeKeybindings)
};

// Per-site settings, an emberstore collection keyed by host. A host also
// covers its subdomains ("google.com" covers mail.google.com). First run
// seeds Gmail, whose own shortcuts clash with the bindings.
struct SitesStore
{
    explicit SitesStore(const emberstore::Database& db);

    // host -> excluded keys, as the JSON object PageScript.h takes.
    std::string excludedKeybindingsJSON();

    void seedDefaults();

    emberstore::Collection<SiteSettings> collection;
};
} // namespace grazer
