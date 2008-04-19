-- Code Poster 2
-- Version: 2.1
-- Author: mozers�  (���� � ������ ����������: VladVRO)
---------------------------------------------------
-- Description:
-- ������������ ���������� ����� ��� ���� ���� � ��������������� ����� ������,
-- � ������ ������������ � ��������� � ��������� ������� (color, bold, italics)

-- ��� ����������� �������� � ���� ���� .properties ��������� ������:
--  command.name.125.*=������������� � ��� ��� ������
--  command.125.*=dofile $(SciteDefaultHome)\tools\code-poster2.lua
--  command.mode.125.*=subsystem:lua,savebefore:no

-- ������� �� �������������:
--  � ������� ������������ ������� editor.LexerLanguage � os.msgbox
--  (������ Ru-Board, http://code.google.com/p/scite-ru)
---------------------------------------------------

local function GetStyleString(pos)
	local style = editor.StyleAt[pos]
	local lang = editor.LexerLanguage
	return props["style."..lang.."."..style]
end

local function GetColor(style_string)
	local fore
	for w in string.gmatch(style_string, "fore: *(#%x%x%x%x%x%x)") do
		fore = w
	end
	return fore
end

local function GetAttr(style_string, attr)
	if string.find(style_string, attr) then
		return true
	else
		return false
	end
end

local function ReplaceForumTag(pos)
	local tag = editor:textrange(pos+1, pos+3)
	if string.sub(tag, 1, 1) == "/" then
		tag = editor:textrange(pos+2, pos+4)
	end
	if tag == "b]" or tag == "i]" or tag == "s]" or tag == "u]" or tag == "st" or tag == "c]" or tag == "ce" or tag == "su" or tag == "si" or tag == "co" or tag == "fo" or tag == "qu" or tag == "q]" or tag == "no" or tag == "hr" or tag == "ur" or tag == "em" or tag == "im" or tag == "li" or tag == "*]" or tag == "ta" or tag == "tr" or tag == "br" or tag == "#]" or tag == "mo" then
		return "[no][[/no]"
	else
		return "["
	end
end

-----------------------------------

local sel_start = editor.SelectionStart
local sel_end = editor.SelectionEnd
local line_start = editor:LineFromPosition(sel_start)+1
-- ���� ������ �� ��������, �� ����� ���� �����
if sel_start == sel_end then
	line_start = 0
	sel_start = 0
	sel_end = editor:PositionFromLine(editor.LineCount)
end

local fore
local fore_old = nil
local italics
local italics_old = false
local bold
local bold_old = false
local forum_text =""
-----------------------------------
for i = sel_start, sel_end-1 do
	local char = editor:textrange(i,i+1)
	if char == "\t" then char = string.rep(" ", props["tabsize"]) end
	if char == "[" then char = ReplaceForumTag(i) end
	if not string.find(char,"%s") then
		local style_string = GetStyleString(i)
		--------------------------------------------
		italics = GetAttr(style_string, "italics")
		if italics ~= italics_old then
			if italics then
				forum_text = forum_text.."[i]"
			else
				forum_text = forum_text.."[/i]"
			end
			italics_old = italics
		end
		--------------------------------------------
		bold = GetAttr(style_string, "bold")
		if bold ~= bold_old then
			if bold then
				forum_text = forum_text.."[b]"
			else
				forum_text = forum_text.."[/b]"
			end
			bold_old = bold
		end
		--------------------------------------------
		fore = GetColor(style_string)
		if fore ~= fore_old and fore_old ~= nil then
			forum_text = forum_text.."[/color]"
		end
		if fore ~= fore_old and fore ~= nil then
			forum_text = forum_text.."[color="..fore.."]"
		end
		fore_old = fore
	end
	forum_text = forum_text..char
end
-----------------------------------
if fore ~= nil then
	forum_text = forum_text.."[/color]"
end
if italics then
	forum_text = forum_text.."[/i]"
end
if bold then
	forum_text = forum_text.."[/b]"
end
-----------------------------------

local header = "[b][color=Blue]"..props["FileNameExt"].."[/color][/b]"
if line_start ~= 0 then
	header = header.." [s][[b]������ "..line_start.."[/b]][/s]"
end
local more = ""
local more_end = ""
if editor:LineFromPosition(sel_end) - line_start > 10 then
	more = "[more]"
	more_end = "[/more]"
end

forum_text = header.." : "..more.."[code]"..forum_text.."[/code]"..more_end
editor:CopyText(forum_text)
shell.msgbox ("��� ��� ������ ������� �����������\n � ������� � ����� ������", "������������ ���� ��� ������")
