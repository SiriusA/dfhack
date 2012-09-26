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

	// Select unit at cursor.
	df::unit * targetUnit = NULL;
	targetUnit = Gui::getSelectedUnit(out);

	if(targetUnit == NULL)
	{
		out.printerr("No unit found at cursor.");
	}

	// Unit found.
	out << "Unit found at: " << targetUnit->pos.x << ", " << targetUnit->pos.y << ", "<< targetUnit->pos.z <<endl;

	MapCache mc;
	// Move unit.
	if(move)
	{

	
		//(*targetUnit).old_pos = targetUnit->pos;
		df::tile_occupancy oldPosOccupancy = mc.occupancyAt(targetUnit->pos);
		df::tile_occupancy newPosOccupancy = mc.occupancyAt(movedCoord);
		if(targetUnit->flags1.bits.on_ground != 0)
		{
			if(oldPosOccupancy.bits.unit == 1)
			{
				size_t numUnits = world->units.all.size();
				size_t unitsAtCursor = 0;	
				for(size_t i=0; i< numUnits; i++)
				{
					DFCoord pos_unit(world->units.all[i]->pos.x, world->units.all[i]->pos.y, world->units.all[i]->pos.z);
					
					if (pos_unit == pos_cursor)
						unitsAtCursor++;
				}
				if(unitsAtCursor <= 2)
				{
					oldPosOccupancy.bits.unit_grounded = 0;
				}
			}
			mc.setOccupancyAt(targetUnit->pos, oldPosOccupancy);
			//Teleporting will dump creatures on their butts.
			newPosOccupancy.bits.unit_grounded = 1;
			mc.setOccupancyAt(movedCoord, newPosOccupancy);
		}
		if(targetUnit->flags1.bits.on_ground == 0)
		{
			//Only one unit can stand on a tile, correct?
			oldPosOccupancy.bits.unit = 0;
			//Teleporting will dump creatures on their butts.
			targetUnit->flags1.bits.on_ground = 1; 
			newPosOccupancy.bits.unit_grounded = 1;
			mc.setOccupancyAt(movedCoord, newPosOccupancy);
		}
		


		(*targetUnit).pos = movedCoord;
		out << "Moved to: " << targetUnit->pos.x << ", " << targetUnit->pos.y << ", "<< targetUnit->pos.z <<endl;
	}

	// Update map.
	mc.WriteAll();

	// Report success.
	return CR_OK;
}