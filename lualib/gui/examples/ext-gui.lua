--[[----------------------------------------------------------
SciTE GUI Extensions for Windows by Steve Donovan
http://mysite.mweb.co.za/residents/sdonovan/scite/gui_ext.zip
Examples of some of the functions
--]]----------------------------------------------------------

-- Set:
require("gui")
-- BUG: After that command does not work in the national character input keyboard layout.

-- NOTE: It's important that any controls be created immediately after the form, so they pick up the correct parent window!

----------
-- WINDOW
----------
-- Set:
window1 = gui.window("title")
window1:size(width, height)
window1:position(x, y)
window1:show()
window1:ontop(true) -- Note: There is no. This - the wish.
window1:hide()
window1:context_menu {'item1|func1', 'item2|func2'} -- Note: Need to use global functions!

-- Get:
visible, x, y, width, height = window1:bounds()

---------
-- PANEL
---------
-- Set:
panel1 = gui.panel(width)
gui.set_panel(panel1, "right") -- "right" or "left"
-- or
window_or_panel1:client(panel2)
panel1:size(width, height) -- Note: Changes either height or width (one of parameters is ignored)
panel1:show() -- BUG: Not working! (it is always shown without this command)
panel1:hide() -- BUG: Not working!

-- Get:
visible, x, y, width, height = panel1:bounds()

----------
-- TABBAR
----------
-- Set:
tabbar1 = gui.tabbar(window_or_panel1)

-- Add:
tabbar1:add_tab("Tab1 heading", list1) -- list1, memo_text1 or panel2

-- Event:
tabbar1:on_select(function(index) print(index) end)

--------
-- LIST
--------
-- Set:
list1 = gui.list(true) -- or false (List heading show)
window_or_panel1:add(list1, "top", height) -- "top" or "bottom"
-- or
window_or_panel1:add(list1, "left", width) -- "left" or "right"
-- or
window_or_panel1:client(list1)
list1:size(width, height) -- Note: Changes either height or width (one of parameters is ignored)
list1:set_list_colour("#FFFFFF", "#000000") -- foreground, background
list1:context_menu {'item1|func1', 'item2|func2'} -- Note: There is no. This - the wish.

-- Add:
list1:add_column('Title1', width) -- Note: If gui.list(true)
list1:add_item ({'Caption1','Caption2'}, {data1, data2}) -- data or table, string, function

-- Change:
list1:delete_item(index)
list1:insert_item(index, {'Caption1','Caption2'}, {data1, data2}) -- data or table, string, function

-- Get:
list1_count = list1:count()
data = list1:get_item_data(index) -- Note: Check index ~= -1
text = list1:get_item_text(index) -- Note: Check index ~= -1. BUG: Returned contain only first entry!
visible, x, y, width, height = list1:bounds()

-- Event:
list1:on_select(function(index) print(index) end) -- Note: Works as on_click. Check index ~= index_old
list1:on_double_click(function(index) print(index) end)

-------------
-- MEMO TEXT
-------------
-- Set:
text1 = gui.memo()
window_or_panel1:add(text1, "top", height) -- "top" or "bottom"
-- or
window_or_panel1:add(text1, "left", width) -- "left" or "right"
-- or
window_or_panel1:client(text1)
text1:size(width, height) -- Note: Changes either height or width (one of parameters is ignored)
text1:set_text('{\\rtf{\\fonttbl{\\f0\\fcharset0 Helv;}}\\f0\\fs16'..'sample text'..'}')
text1:set_list_colour("#FFFFFF", "#000000") -- foreground, background

-- Get:
visible, x, y, width, height = text1:bounds()
text = text1:get_text() -- Note: There is no. This - the wish.

-- Event:
list1:on_key(function(key) print(key) end) -- Note: There is no. This - the wish.