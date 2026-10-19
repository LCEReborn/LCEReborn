#ifndef PLATFORM_PC_PCLICENSEDEFAULTS_HPP
#define PLATFORM_PC_PCLICENSEDEFAULTS_HPP

static const char kPcDefaultLicenseConfig[] =
    "# What the PC port unlocks.\n"
    "#\n"
    "# The PC has no store and no way to check what you bought, so nothing is\n"
    "# unlocked until you edit this file. Only unlock what you own.\n"
    "#\n"
    "# full_game: true unlocks the full game; false plays the trial.\n"
    "# dlc: none, all, or pack names separated by commas, such as\n"
    "#      Festive Skin Pack, City Texture Pack\n"
    "\n"
    "full_game = false\n"
    "dlc = none\n";

#endif
