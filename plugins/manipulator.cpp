// Dwarf Manipulator - a Therapist-style labor editor

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Screen.h>
#include <modules/Translation.h>
#include <modules/Units.h>
#include <vector>
#include <string>
#include <set>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/ui.h"
#include "df/graphic.h"
#include "df/enabler.h"
#include "df/viewscreen_unitlistst.h"
#include "df/interface_key.h"
#include "df/unit.h"
#include "df/unit_soul.h"
#include "df/unit_skill.h"
#include "df/creature_raw.h"
#include "df/caste_raw.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::gps;
using df::global::enabler;

DFHACK_PLUGIN("manipulator");

struct SkillLevel
{
	const char *name;
	int points;
	char abbrev;
};

#define    NUM_SKILL_LEVELS (sizeof(skill_levels) / sizeof(SkillLevel))

// The various skill rankings. Zero skill is hardcoded to "Not" and '-'.
const SkillLevel skill_levels[] = {
	{"Dabbling",	500, '0'},
	{"Novice",	600, '1'},
	{"Adequate",	700, '2'},
	{"Competent",	800, '3'},
	{"Skilled",	900, '4'},
	{"Proficient",	1000, '5'},
	{"Talented",	1100, '6'},
	{"Adept",	1200, '7'},
	{"Expert",	1300, '8'},
	{"Professional",1400, '9'},
	{"Accomplished",1500, 'A'},
	{"Great",	1600, 'B'},
	{"Master",	1700, 'C'},
	{"High Master",	1800, 'D'},
	{"Grand Master",1900, 'E'},
	{"Legendary",	2000, 'U'},
	{"Legendary+1",	2100, 'V'},
	{"Legendary+2",	2200, 'W'},
	{"Legendary+3",	2300, 'X'},
	{"Legendary+4",	2400, 'Y'},
	{"Legendary+5",	0, 'Z'}
};

struct SkillColumn
{
    df::profession profession;
    df::unit_labor labor;
    df::job_skill skill;
    char label[3];
    bool special; // specified labor is mutually exclusive with all other special labors
};

#define    NUM_COLUMNS (sizeof(columns) / sizeof(SkillColumn))

// All of the skill/labor columns we want to display. Includes profession (for color), labor, skill, and 2 character label
const SkillColumn columns[] = {
// Mining
    {profession::MINER, unit_labor::MINE, job_skill::MINING, "Mi", true},
// Woodworking
    {profession::WOODWORKER, unit_labor::CARPENTER, job_skill::CARPENTRY, "Ca"},
    {profession::WOODWORKER, unit_labor::BOWYER, job_skill::BOWYER, "Bw"},
    {profession::WOODWORKER, unit_labor::CUTWOOD, job_skill::WOODCUTTING, "WC", true},
// Stoneworking
    {profession::STONEWORKER, unit_labor::MASON, job_skill::MASONRY, "Ma"},
    {profession::STONEWORKER, unit_labor::DETAIL, job_skill::DETAILSTONE, "En"},
// Hunting/Related
    {profession::RANGER, unit_labor::ANIMALTRAIN, job_skill::ANIMALTRAIN, "Tr"},
    {profession::RANGER, unit_labor::ANIMALCARE, job_skill::ANIMALCARE, "Ca"},
    {profession::RANGER, unit_labor::HUNT, job_skill::SNEAK, "Hu", true},
    {profession::RANGER, unit_labor::TRAPPER, job_skill::TRAPPING, "Tr"},
    {profession::RANGER, unit_labor::DISSECT_VERMIN, job_skill::DISSECT_VERMIN, "Di"},
// Healthcare
    {profession::DOCTOR, unit_labor::DIAGNOSE, job_skill::DIAGNOSE, "Di"},
    {profession::DOCTOR, unit_labor::SURGERY, job_skill::SURGERY, "Su"},
    {profession::DOCTOR, unit_labor::BONE_SETTING, job_skill::SET_BONE, "Bo"},
    {profession::DOCTOR, unit_labor::SUTURING, job_skill::SUTURE, "St"},
    {profession::DOCTOR, unit_labor::DRESSING_WOUNDS, job_skill::DRESS_WOUNDS, "Dr"},
    {profession::DOCTOR, unit_labor::FEED_WATER_CIVILIANS, job_skill::NONE, "Fd"},
    {profession::DOCTOR, unit_labor::RECOVER_WOUNDED, job_skill::NONE, "Re"},
// Farming/Related
    {profession::FARMER, unit_labor::BUTCHER, job_skill::BUTCHER, "Bu"},
    {profession::FARMER, unit_labor::TANNER, job_skill::TANNER, "Ta"},
    {profession::FARMER, unit_labor::PLANT, job_skill::PLANT, "Gr"},
    {profession::FARMER, unit_labor::DYER, job_skill::DYER, "Dy"},
    {profession::FARMER, unit_labor::SOAP_MAKER, job_skill::SOAP_MAKING, "So"},
    {profession::FARMER, unit_labor::BURN_WOOD, job_skill::WOOD_BURNING, "WB"},
    {profession::FARMER, unit_labor::POTASH_MAKING, job_skill::POTASH_MAKING, "Po"},
    {profession::FARMER, unit_labor::LYE_MAKING, job_skill::LYE_MAKING, "Ly"},
    {profession::FARMER, unit_labor::MILLER, job_skill::MILLING, "Ml"},
    {profession::FARMER, unit_labor::BREWER, job_skill::BREWING, "Br"},
    {profession::FARMER, unit_labor::HERBALIST, job_skill::HERBALISM, "He"},
    {profession::FARMER, unit_labor::PROCESS_PLANT, job_skill::PROCESSPLANTS, "Th"},
    {profession::FARMER, unit_labor::MAKE_CHEESE, job_skill::CHEESEMAKING, "Ch"},
    {profession::FARMER, unit_labor::MILK, job_skill::MILK, "Mk"},
    {profession::FARMER, unit_labor::SHEARER, job_skill::SHEARING, "Sh"},
    {profession::FARMER, unit_labor::SPINNER, job_skill::SPINNING, "Sp"},
    {profession::FARMER, unit_labor::COOK, job_skill::COOK, "Co"},
    {profession::FARMER, unit_labor::PRESSING, job_skill::PRESSING, "Pr"},
    {profession::FARMER, unit_labor::BEEKEEPING, job_skill::BEEKEEPING, "Be"},
// Fishing/Related
    {profession::FISHERMAN, unit_labor::FISH, job_skill::FISH, "Fi"},
    {profession::FISHERMAN, unit_labor::CLEAN_FISH, job_skill::PROCESSFISH, "Cl"},
    {profession::FISHERMAN, unit_labor::DISSECT_FISH, job_skill::DISSECT_FISH, "Di"},
// Metalsmithing
    {profession::METALSMITH, unit_labor::SMELT, job_skill::SMELT, "Fu"},
    {profession::METALSMITH, unit_labor::FORGE_WEAPON, job_skill::FORGE_WEAPON, "We"},
    {profession::METALSMITH, unit_labor::FORGE_ARMOR, job_skill::FORGE_ARMOR, "Ar"},
    {profession::METALSMITH, unit_labor::FORGE_FURNITURE, job_skill::FORGE_FURNITURE, "Bl"},
    {profession::METALSMITH, unit_labor::METAL_CRAFT, job_skill::METALCRAFT, "Cr"},
// Jewelry
    {profession::JEWELER, unit_labor::CUT_GEM, job_skill::CUTGEM, "Cu"},
    {profession::JEWELER, unit_labor::ENCRUST_GEM, job_skill::ENCRUSTGEM, "Se"},
// Crafts
    {profession::CRAFTSMAN, unit_labor::LEATHER, job_skill::LEATHERWORK, "Le"},
    {profession::CRAFTSMAN, unit_labor::WOOD_CRAFT, job_skill::WOODCRAFT, "Wo"},
    {profession::CRAFTSMAN, unit_labor::STONE_CRAFT, job_skill::STONECRAFT, "St"},
    {profession::CRAFTSMAN, unit_labor::BONE_CARVE, job_skill::BONECARVE, "Bo"},
    {profession::CRAFTSMAN, unit_labor::GLASSMAKER, job_skill::GLASSMAKER, "Gl"},
    {profession::CRAFTSMAN, unit_labor::WEAVER, job_skill::WEAVING, "We"},
    {profession::CRAFTSMAN, unit_labor::CLOTHESMAKER, job_skill::CLOTHESMAKING, "Cl"},
    {profession::CRAFTSMAN, unit_labor::EXTRACT_STRAND, job_skill::EXTRACT_STRAND, "Ad"},
    {profession::CRAFTSMAN, unit_labor::POTTERY, job_skill::POTTERY, "Po"},
    {profession::CRAFTSMAN, unit_labor::GLAZING, job_skill::GLAZING, "Gl"},
    {profession::CRAFTSMAN, unit_labor::WAX_WORKING, job_skill::WAX_WORKING, "Wx"},
// Engineering
    {profession::ENGINEER, unit_labor::SIEGECRAFT, job_skill::SIEGECRAFT, "En"},
    {profession::ENGINEER, unit_labor::SIEGEOPERATE, job_skill::SIEGEOPERATE, "Op"},
    {profession::ENGINEER, unit_labor::MECHANIC, job_skill::MECHANICS, "Me"},
    {profession::ENGINEER, unit_labor::OPERATE_PUMP, job_skill::OPERATE_PUMP, "Pu"},
// Hauling
    {profession::STANDARD, unit_labor::HAUL_STONE, job_skill::NONE, "St"},
    {profession::STANDARD, unit_labor::HAUL_WOOD, job_skill::NONE, "Wo"},
    {profession::STANDARD, unit_labor::HAUL_ITEM, job_skill::NONE, "It"},
    {profession::STANDARD, unit_labor::HAUL_BODY, job_skill::NONE, "Bu"},
    {profession::STANDARD, unit_labor::HAUL_FOOD, job_skill::NONE, "Fo"},
    {profession::STANDARD, unit_labor::HAUL_REFUSE, job_skill::NONE, "Re"},
    {profession::STANDARD, unit_labor::HAUL_FURNITURE, job_skill::NONE, "Fu"},
    {profession::STANDARD, unit_labor::HAUL_ANIMAL, job_skill::NONE, "An"},
    {profession::STANDARD, unit_labor::PUSH_HAUL_VEHICLE, job_skill::NONE, "Ve"},
// Other Jobs
    {profession::CHILD, unit_labor::ARCHITECT, job_skill::DESIGNBUILDING, "Ar"},
    {profession::CHILD, unit_labor::ALCHEMIST, job_skill::ALCHEMY, "Al"},
    {profession::CHILD, unit_labor::CLEAN, job_skill::NONE, "Cl"},
// Military
    {profession::WRESTLER, unit_labor::NONE, job_skill::WRESTLING, "Wr"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::AXE, "Ax"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::SWORD, "Sw"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::MACE, "Mc"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::HAMMER, "Ha"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::SPEAR, "Sp"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::DAGGER, "Kn"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::CROSSBOW, "Cb"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::BOW, "Bo"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::BLOWGUN, "Bl"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::PIKE, "Pk"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::WHIP, "La"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::ARMOR, "Ar"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::SHIELD, "Sh"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::BITE, "Bi"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::GRASP_STRIKE, "Pu"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::STANCE_STRIKE, "Ki"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::DODGING, "Do"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::MISC_WEAPON, "Mi"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::MELEE_COMBAT, "Fi"},
    {profession::WRESTLER, unit_labor::NONE, job_skill::RANGED_COMBAT, "Ar"},

    {profession::RECRUIT, unit_labor::NONE, job_skill::LEADERSHIP, "Le"},
    {profession::RECRUIT, unit_labor::NONE, job_skill::TEACHING, "Te"},
    {profession::RECRUIT, unit_labor::NONE, job_skill::KNOWLEDGE_ACQUISITION, "Lr"},
    {profession::RECRUIT, unit_labor::NONE, job_skill::DISCIPLINE, "Di"},
    {profession::RECRUIT, unit_labor::NONE, job_skill::CONCENTRATION, "Co"},
    {profession::RECRUIT, unit_labor::NONE, job_skill::SITUATIONAL_AWARENESS, "Aw"},
    {profession::RECRUIT, unit_labor::NONE, job_skill::COORDINATION, "Cr"},
    {profession::RECRUIT, unit_labor::NONE, job_skill::BALANCE, "Ba"},

// Social
    {profession::STANDARD, unit_labor::NONE, job_skill::PERSUASION, "Pe"},
    {profession::STANDARD, unit_labor::NONE, job_skill::NEGOTIATION, "Ne"},
    {profession::STANDARD, unit_labor::NONE, job_skill::JUDGING_INTENT, "Ju"},
    {profession::STANDARD, unit_labor::NONE, job_skill::LYING, "Ly"},
    {profession::STANDARD, unit_labor::NONE, job_skill::INTIMIDATION, "In"},
    {profession::STANDARD, unit_labor::NONE, job_skill::CONVERSATION, "Cn"},
    {profession::STANDARD, unit_labor::NONE, job_skill::COMEDY, "Cm"},
    {profession::STANDARD, unit_labor::NONE, job_skill::FLATTERY, "Fl"},
    {profession::STANDARD, unit_labor::NONE, job_skill::CONSOLE, "Cs"},
    {profession::STANDARD, unit_labor::NONE, job_skill::PACIFY, "Pc"},
    {profession::ADMINISTRATOR, unit_labor::NONE, job_skill::APPRAISAL, "Ap"},
    {profession::ADMINISTRATOR, unit_labor::NONE, job_skill::ORGANIZATION, "Or"},
    {profession::ADMINISTRATOR, unit_labor::NONE, job_skill::RECORD_KEEPING, "RK"},

// Miscellaneous
    {profession::STANDARD, unit_labor::NONE, job_skill::THROW, "Th"},
    {profession::STANDARD, unit_labor::NONE, job_skill::CRUTCH_WALK, "CW"},
    {profession::STANDARD, unit_labor::NONE, job_skill::SWIMMING, "Sw"},
    {profession::STANDARD, unit_labor::NONE, job_skill::KNAPPING, "Kn"},

    {profession::DRUNK, unit_labor::NONE, job_skill::WRITING, "Wr"},
    {profession::DRUNK, unit_labor::NONE, job_skill::PROSE, "Pr"},
    {profession::DRUNK, unit_labor::NONE, job_skill::POETRY, "Po"},
    {profession::DRUNK, unit_labor::NONE, job_skill::READING, "Rd"},
    {profession::DRUNK, unit_labor::NONE, job_skill::SPEAKING, "Sp"},

    {profession::ADMINISTRATOR, unit_labor::NONE, job_skill::MILITARY_TACTICS, "MT"},
    {profession::ADMINISTRATOR, unit_labor::NONE, job_skill::TRACKING, "Tr"},
    {profession::ADMINISTRATOR, unit_labor::NONE, job_skill::MAGIC_NATURE, "Dr"},
};

struct UnitInfo
{
    df::unit *unit;
    bool allowEdit;
    string name;
    string transname;
    string profession;
    int8_t color;
};

#define    FILTER_NONWORKERS    0x0001
#define    FILTER_NONDWARVES    0x0002
#define    FILTER_NONCIV        0x0004
#define    FILTER_ANIMALS        0x0008
#define    FILTER_LIVING        0x0010
#define    FILTER_DEAD        0x0020

class viewscreen_unitlaborsst : public dfhack_viewscreen {
public:
    static viewscreen_unitlaborsst *create (char pushtype, df::viewscreen *scr = NULL);

    void feed(set<df::interface_key> *events);

    void render();
    void resize(int w, int h) { calcSize(); }

    void help() { }

    std::string getFocusString() { return "unitlabors"; }

    viewscreen_unitlaborsst();
    ~viewscreen_unitlaborsst() { };

protected:
    vector<UnitInfo *> units;
    int filter;

    int first_row, sel_row;
    int first_column, sel_column;

    int height, name_width, prof_width, labors_width;

//    bool descending;
//    int sort_skill;
//    int sort_labor;

    void readUnits ();
    void calcSize ();
};

viewscreen_unitlaborsst::viewscreen_unitlaborsst()
{
    filter = FILTER_LIVING;
    first_row = sel_row = 0;
    first_column = sel_column = 0;
    calcSize();
    readUnits();
}

void viewscreen_unitlaborsst::readUnits ()
{
    for (size_t i = 0; i < units.size(); i++)
        delete units[i];
    units.clear();

    UnitInfo *cur = new UnitInfo;
    for (size_t i = 0; i < world->units.active.size(); i++)
    {
        df::unit *unit = world->units.active[i];
        cur->unit = unit;
        cur->allowEdit = true;

        if (unit->race != ui->race_id)
        {
            cur->allowEdit = false;
            if (!(filter & FILTER_NONDWARVES))
                continue;
        }

        if (unit->civ_id != ui->civ_id)
        {
            cur->allowEdit = false;
            if (!(filter & FILTER_NONCIV))
                continue;
        }

        if (unit->flags1.bits.dead)
        {
            cur->allowEdit = false;
            if (!(filter & FILTER_DEAD))
                continue;
        }
        else
        {
            if (!(filter & FILTER_LIVING))
                continue;
        }

        if (!ENUM_ATTR(profession, can_assign_labor, unit->profession))
        {
            cur->allowEdit = false;
            if (!(filter & FILTER_NONWORKERS))
                continue;
        }

        if (!unit->name.first_name.length())
        {
            if (!(filter & FILTER_ANIMALS))
                continue;
        }

        cur->name = Translation::TranslateName(&unit->name, false);
        cur->transname = Translation::TranslateName(&unit->name, true);
        cur->profession = Units::getProfessionName(unit);
        cur->color = Units::getProfessionColor(unit);

        units.push_back(cur);
        cur = new UnitInfo;
    }
    delete cur;
}

void viewscreen_unitlaborsst::calcSize()
{
    height = gps->dimy - 10;
    if (height > units.size())
        height = units.size();

    name_width = prof_width = labors_width = 0;
    for (int i = 4; i < gps->dimx; i++)
    {
        // 20% for Name, 20% for Profession, 60% for Labors
        switch ((i - 4) % 5)
        {
        case 0: case 2: case 4:
            labors_width++;
            break;
        case 1:
            name_width++;
            break;
        case 3:
            prof_width++;
            break;
        }
    }
    while (labors_width > NUM_COLUMNS)
    {
        if (labors_width & 1)
            name_width++;
        else
            prof_width++;
        labors_width--;
    }

    // don't adjust scroll position immediately after the window opened
    if (units.size() == 0)
        return;

    // if the window grows vertically, scroll upward to eliminate blank rows from the bottom
    if (first_row > units.size() - height)
        first_row = units.size() - height;

    // if it shrinks vertically, scroll downward to keep the cursor visible
    if (first_row < sel_row - height + 1)
        first_row = sel_row - height + 1;

    // if the window grows horizontally, scroll to the left to eliminate blank columns from the right
    if (first_column > NUM_COLUMNS - labors_width)
        first_column = NUM_COLUMNS - labors_width;

    // if it shrinks horizontally, scroll to the right to keep the cursor visible
    if (first_column < sel_column - labors_width + 1)
        first_column = sel_column - labors_width + 1;
}

void viewscreen_unitlaborsst::feed(set<df::interface_key> *events)
{
    if (events->count(interface_key::LEAVESCREEN))
    {
        events->clear();
        Screen::dismiss(this);
        return;
    }

    // TODO - allow modifying filters

    if (!units.size())
        return;

    if (events->count(interface_key::CURSOR_UP) || events->count(interface_key::CURSOR_UPLEFT) || events->count(interface_key::CURSOR_UPRIGHT))
        sel_row--;
    if (events->count(interface_key::CURSOR_UP_FAST) || events->count(interface_key::CURSOR_UPLEFT_FAST) || events->count(interface_key::CURSOR_UPRIGHT_FAST))
        sel_row -= 10;
    if (events->count(interface_key::CURSOR_DOWN) || events->count(interface_key::CURSOR_DOWNLEFT) || events->count(interface_key::CURSOR_DOWNRIGHT))
        sel_row++;
    if (events->count(interface_key::CURSOR_DOWN_FAST) || events->count(interface_key::CURSOR_DOWNLEFT_FAST) || events->count(interface_key::CURSOR_DOWNRIGHT_FAST))
        sel_row += 10;

    if (sel_row < 0)
        sel_row = 0;
    if (sel_row > units.size() - 1)
        sel_row = units.size() - 1;

    if (sel_row < first_row)
        first_row = sel_row;
    if (first_row < sel_row - height + 1)
        first_row = sel_row - height + 1;

    if (events->count(interface_key::CURSOR_LEFT) || events->count(interface_key::CURSOR_UPLEFT) || events->count(interface_key::CURSOR_DOWNLEFT))
        sel_column--;
    if (events->count(interface_key::CURSOR_LEFT_FAST) || events->count(interface_key::CURSOR_UPLEFT_FAST) || events->count(interface_key::CURSOR_DOWNLEFT_FAST))
        sel_column -= 10;
    if (events->count(interface_key::CURSOR_RIGHT) || events->count(interface_key::CURSOR_UPRIGHT) || events->count(interface_key::CURSOR_DOWNRIGHT))
        sel_column++;
    if (events->count(interface_key::CURSOR_RIGHT_FAST) || events->count(interface_key::CURSOR_UPRIGHT_FAST) || events->count(interface_key::CURSOR_DOWNRIGHT_FAST))
        sel_column += 10;

    if ((sel_column != 0) && events->count(interface_key::CURSOR_UP_Z))
    {
        // go to beginning of current column group; if already at the beginning, go to the beginning of the previous one
        sel_column--;
        df::profession cur = columns[sel_column].profession;
        while ((sel_column > 0) && columns[sel_column - 1].profession == cur)
            sel_column--;
    }
    if ((sel_column != NUM_COLUMNS - 1) && events->count(interface_key::CURSOR_DOWN_Z))
    {
        // go to end of current column group; if already at the end, go to the end of the next one
        sel_column++;
        df::profession cur = columns[sel_column].profession;
        while ((sel_column < NUM_COLUMNS - 1) && columns[sel_column + 1].profession == cur)
            sel_column++;
    }

    if (sel_column < 0)
        sel_column = 0;
    if (sel_column > NUM_COLUMNS - 1)
        sel_column = NUM_COLUMNS - 1;

    if (sel_column < first_column)
        first_column = sel_column;
    if (first_column < sel_column - labors_width + 1)
        first_column = sel_column - labors_width + 1;

    UnitInfo *cur = units[sel_row];
    if (events->count(interface_key::SELECT) && (cur->allowEdit) && (columns[sel_column].labor != unit_labor::NONE))
    {
        df::unit *unit = cur->unit;
        const SkillColumn &col = columns[sel_column];
        if (col.special)
        {
            if (!unit->status.labors[col.labor])
            {
                for (int i = 0; i < NUM_COLUMNS; i++)
                {
                    if ((columns[i].labor != unit_labor::NONE) && columns[i].special)
                        unit->status.labors[columns[i].labor] = false;
                }
            }
            unit->military.pickup_flags.bits.update = true;
        }
        unit->status.labors[col.labor] = !unit->status.labors[col.labor];
    }

    // TODO: add sorting
}

void viewscreen_unitlaborsst::render()
{
    if (Screen::isDismissed(this))
        return;

    dfhack_viewscreen::render();

    Screen::clear();
    Screen::drawBorder("  Manage Labors  ");

    for (int col = 0; col < labors_width; col++)
    {
        int col_offset = col + first_column;
        if (col_offset >= NUM_COLUMNS)
            break;

        int8_t fg = Units::getCasteProfessionColor(ui->race_id, -1, columns[col_offset].profession);
        int8_t bg = 0;

        if (col_offset == sel_column)
        {
            fg = 0;
            bg = 7;
        }
        Screen::paintTile(Screen::Pen(columns[col_offset].label[0], fg, bg), 1 + name_width + 1 + prof_width + 1 + col, 1);
        Screen::paintTile(Screen::Pen(columns[col_offset].label[1], fg, bg), 1 + name_width + 1 + prof_width + 1 + col, 2);
    }

    for (int row = 0; row < height; row++)
    {
        int row_offset = row + first_row;
        if (row_offset >= units.size())
            break;
        UnitInfo *cur = units[row_offset];
        df::unit *unit = cur->unit;
        int8_t fg = 15, bg = 0;
        if (row_offset == sel_row)
        {
            fg = 0;
            bg = 7;
        }

        string name = cur->name;
        name.resize(name_width);
        Screen::paintString(Screen::Pen(' ', fg, bg), 1, 3 + row, name);

        string profession = cur->profession;
        profession.resize(prof_width);
        fg = cur->color;
        bg = 0;
        Screen::paintString(Screen::Pen(' ', fg, bg), 1 + prof_width + 1, 3 + row, profession);

        // Print unit's skills and labor assignments
        for (int col = 0; col < labors_width; col++)
        {
            int col_offset = col + first_column;
            fg = 15;
            bg = 0;
            if ((col_offset == sel_column) && (row_offset == sel_row))
                fg = 9;
            if ((columns[col_offset].labor != unit_labor::NONE) && (unit->status.labors[columns[col_offset].labor]))
                bg = 7;
            char c = '-';
            if (columns[col_offset].skill != job_skill::NONE)
            {
                df::unit_skill *skill = binsearch_in_vector<df::unit_skill,df::enum_field<df::job_skill,int16_t>>(unit->status.current_soul->skills, &df::unit_skill::id, columns[col_offset].skill);
                if ((skill != NULL) && (skill->rating || skill->experience))
                {
                    int level = skill->rating;
                    if (level > NUM_SKILL_LEVELS - 1)
                        level = NUM_SKILL_LEVELS - 1;
                    c = skill_levels[level].abbrev;
                }
            }
            Screen::paintTile(Screen::Pen(c, fg, bg), 1 + name_width + 1 + prof_width + 1 + col, 3 + row);
        }
    }

    UnitInfo *cur = units[sel_row];
    if (cur != NULL)
    {
        df::unit *unit = cur->unit;
        string str = cur->transname;
        if (str.length())
            str += ", ";
        str += cur->profession;
        str += ":";

        Screen::paintString(Screen::Pen(' ', 15, 0), 1, 3 + height + 2, str);
        int y = 1 + str.length() + 1;

        if (columns[sel_column].skill == job_skill::NONE)
        {
            str = ENUM_ATTR_STR(unit_labor, caption, columns[sel_column].labor);
            if (unit->status.labors[columns[sel_column].labor])
                str += " Enabled";
            else
                str += " Not Enabled";
            Screen::paintString(Screen::Pen(' ', 9, 0), y, 3 + height + 2, str);
        }
        else
        {
            df::unit_skill *skill = binsearch_in_vector<df::unit_skill,df::enum_field<df::job_skill,int16_t>>(unit->status.current_soul->skills, &df::unit_skill::id, columns[sel_column].skill);
            if (skill)
            {
                int level = skill->rating;
                if (level > NUM_SKILL_LEVELS - 1)
                    level = NUM_SKILL_LEVELS - 1;
                str = stl_sprintf("%s %s", skill_levels[level].name, ENUM_ATTR_STR(job_skill, caption_noun, columns[sel_column].skill));
                if (level != NUM_SKILL_LEVELS - 1)
                    str += stl_sprintf(" (%d/%d)", skill->experience, skill_levels[level].points);
            }
            else
                str = stl_sprintf("Not %s (0/500)", ENUM_ATTR_STR(job_skill, caption_noun, columns[sel_column].skill));
            Screen::paintString(Screen::Pen(' ', 9, 0), y, 3 + height + 2, str);
        }
    }

    // TODO - print command help info
}

struct unitlist_hook : df::viewscreen_unitlistst
{
    typedef df::viewscreen_unitlistst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, feed, (set<df::interface_key> *input))
    {
        if (input->count(interface_key::UNITVIEW_PRF_PROF))
        {
            Screen::dismiss(this);
            Screen::show(new viewscreen_unitlaborsst());
            return;
        }
        INTERPOSE_NEXT(feed)(input);
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(unitlist_hook, feed);

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    if (gps)
    {
        if (!INTERPOSE_HOOK(unitlist_hook, feed).apply())
            out.printerr("Could not interpose viewscreen_unitlistst::feed\n");
    }

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    INTERPOSE_HOOK(unitlist_hook, feed).remove();
    return CR_OK;
}
