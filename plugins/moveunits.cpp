// moveunits plugin
// Moves local units by x,y,z coordinates
#include <iostream>
#include <iomanip>
#include <sstream>
#include <climits>
#include <vector>
#include <string>
#include <algorithm>
#include <set>
using namespace std;

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "modules/Maps.h"
#include "modules/Gui.h"
#include "modules/Items.h"
#include "modules/Materials.h"
#include "modules/MapCache.h"
#include "DataDefs.h"
#include "df/world.h"
#include "df/unit.h"
#include "MiscUtils.h"
#include "df/general_ref.h"

#include "DFHack.h"

using namespace DFHack;
using namespace df::enums;

using MapExtras::Block;
using MapExtras::MapCache;
using df::global::world;

DFHACK_PLUGIN("moveunits");

command_result df_moveunits(color_ostream &out, vector <string> & parameters);

const string moveunitsHelp = 
	"moveunits moves unit(s) at the cursor to new locations.\n"
	"Parameters: \n"
	"	None: displays location of unit.\n"
	"	m/move <x> <y> <z>: displays location of unit, then moves unit to location indicated by <x> <y> <z>.\n";

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "moveunits", "Moves units.",
        df_moveunits, false,
        moveunitsHelp.c_str()
    ));

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}


command_result df_moveunits(color_ostream &out, vector <string> & parameters)
{
	//Move parameter. Differentiates whether the unit will be moved or not.
	bool move = false;
	//New coordinates.
	DFCoord movedCoord;


    for (size_t i = 0; i < parameters.size(); i++)
    {
        string & p = parameters[i];
        
        if (p == "help" || p == "?" || p == "h" || p == "/?" || p == "info" || p == "man")
        {
            out << moveunitsHelp << endl;
            return CR_OK;
        }
		else if (p == "move" || p == "m")
		{
			move = true;
			uint16_t coords[3];
			i++;
			for(int j = 0; j < 3 && i < parameters.size(); i++, j++)
			{
				string & q = parameters[i];
				if(isdigit(q.c_str()[0]) && atoi(q.c_str()) >= 0)
				{
					coords[j] = atoi(q.c_str());
				}
				else
				{
					move = false;
					out << "Coordinates invalid." << endl;
					return CR_FAILURE;
				}
			}
			movedCoord = DFCoord(coords[0], coords[1], coords[2]);
		}
        else
        {
            out << p << "Showing location of selected unit." << endl;
        }
    }

	// Ensure that the map information is open
    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }

	// Lookup the cursor position
	int cx, cy, cz;
	DFCoord pos_cursor;

	// needs a cursor
	if (!Gui::getCursorCoords(cx,cy,cz))
	{
		out.printerr("Cursor position not found. Please enable the cursor.\n");
		return CR_FAILURE;
	}
	pos_cursor = DFCoord(cx,cy,cz);
	df::unit cake;

	// Iterate over all units, process the first one whose pos == pos_cursor
	df::unit * targetUnit;
	size_t numUnits = world->units.all.size();
	for(size_t i=0; i< numUnits; i++)
	{
		targetUnit = world->units.all[i];	//Assume a match, then verify
		DFCoord pos_unit(targetUnit->pos.x, targetUnit->pos.y, targetUnit->pos.z);

		if (pos_unit == pos_cursor)
		{
			break;
		}
		if (i + 1 == numUnits)
		{
			out.print("No unit found at cursor. Printing location coordinates.\n");
			out << pos_cursor.x << ", " << pos_cursor.y << ", " << pos_cursor.z <<endl;
			return CR_OK;
		}
	}

	// Unit found.
	out << "Unit found at: " << targetUnit->pos.x << ", " << targetUnit->pos.y << ", "<< targetUnit->pos.z <<endl;

	MapCache mc;
	// Move unit.
	if(move)
	{
		(*targetUnit).old_pos = targetUnit->pos;
		(*targetUnit).pos = movedCoord;
		out << "Moved to: " << targetUnit->pos.x << ", " << targetUnit->pos.y << ", "<< targetUnit->pos.z <<endl;
	}

	// Update map.
	mc.WriteAll();
	
	// Report success.
	return CR_OK;
}
