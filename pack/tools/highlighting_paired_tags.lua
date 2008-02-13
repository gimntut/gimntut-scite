--[[--------------------------------------------------
Highlighting Paired Tags
Version: 1.5
Author: mozers�, VladVRO
------------------------------
��������� ������ � �������� ����� � HTML � XML
� ����� �������� �������� ���� ��������� ������ � �������� �����
��������:
� ������� ������������ ������� �� COMMON.lua (����������� ����������� COMMON.lua)
------------------------------
�����������:
�������� � SciTEStartup.lua ������:
  dofile (props["SciteDefaultHome"].."\\tools\\highlighting_paired_tags.lua")
�������� � ���� �������� ��������:
  hypertext.highlighting.paired.tags=1
������������� ����� ������ � ����� ��������:
  style.marker.pairtags=<����> (��� <����> �������� #0099FF, �� ��������� #0000FF)
  style.marker.unpairedtag=<����> (���� �� �����, �� �������� ���� �� ��������������)
--]]----------------------------------------------------

------[[ I N I T   M A R K S ]]-------------------------

local color1, color2

local function InitMarkStyles()
	color1 = props['style.marker.pairtags']
	if color1 == '' then color1 = '#0000FF' end
	EditorInitMarkStyle(1, INDIC_ROUNDBOX, color1)
	color2 = props['style.marker.unpairedtag']
	if color2 ~= '' then
		EditorInitMarkStyle(2, INDIC_ROUNDBOX, color2)
	end
end

------[[ F I N D   P A I R E D   T A G S ]]-------------

local old_current_pos

local function PairedTagsFinder()
	local current_pos = editor.CurrentPos
	if current_pos == old_current_pos then return end
	old_current_pos = current_pos
	if editor.CharAt[current_pos] == 47 then
		current_pos = current_pos + 1
	end
	local tag_start = editor:WordStartPosition(current_pos, true)
	local tag_end = editor:WordEndPosition(current_pos, true)
	local tag_length = tag_end - tag_start
	EditorClearMarks(0, editor.Length)
	if tag_length > 0 then
		if editor.StyleAt[tag_start] == 1 then
			local tag_paired_start, tag_paired_end, dec, find_end, dt
			if editor.CharAt[tag_start-1] == 47 then
				dec = -1
				find_end = 0
				dt = 1
			else
				dec = 1
				find_end = editor.Length
				dt = 0
			end
			EditorMarkText(tag_start-dt, tag_length+dt, 1) -- Start tag to paint in Blue

			-- Find paired tag
			local tag = editor:textrange(tag_start, tag_end)
			local find_flags = SCFIND_WHOLEWORD and SCFIND_REGEXP
			local find_start = tag_start
			local count = 1
			repeat
				tag_paired_start, tag_paired_end = editor:findtext("</*"..tag, find_flags, find_start, find_end)
				if tag_paired_start == nil then break end
				if editor.CharAt[tag_paired_start+1] == 47 then
					count = count - dec
				else
					count = count + dec
				end
				if count == 0 then break end
				find_start = tag_paired_start + dec
			until false

			if tag_paired_start ~= nil then
				-- Paired tag to paint in Blue
				EditorMarkText(tag_paired_start+1, tag_paired_end-tag_paired_start-1, 1)
			else
				EditorClearMarks(0, editor.Length)
				if color2 ~= '' then
					EditorMarkText(tag_start-dt, tag_length+dt, 2) -- Start tag to paint in Red
				end
			end
		end
	end
end

------[[ H A N D L E R S ]]-------------

-- Add user event handler OnUpdateUI
local old_OnUpdateUI = OnUpdateUI
function OnUpdateUI ()
	local result
	if old_OnUpdateUI then result = old_OnUpdateUI() end
	if tonumber(props["hypertext.highlighting.paired.tags"]) == 1 then
		if editor.LexerLanguage == "hypertext" or editor.LexerLanguage == "xml" then
			PairedTagsFinder()
		end
	end
	return result
end

-- Add user event handler OnOpen
local old_OnOpen = OnOpen
function OnOpen(file)
	local result
	if old_OnOpen then result = old_OnOpen(file) end
	InitMarkStyles()
	return result
end

-- Add user event handler OnSwitchFile
local old_OnSwitchFile = OnSwitchFile
function OnSwitchFile(file)
	local result
	if old_OnSwitchFile then result = old_OnSwitchFile(file) end
	InitMarkStyles()
	PairedTagsFinder()
	return result
end
