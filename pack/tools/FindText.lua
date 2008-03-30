--[[--------------------------------------------------
FindText v6.6
������: mozers�, mimir, �������, codewarlock1101

* ���� ����� ������� - ������ ���������� ���������
* ���� ����� �� ������� - ������ ������� �����
* ����� �������� ��� � ���� ��������������, ��� � � ���� �������
* ������, ���������� ���������� ������, ��������� � �������
* ����������� �� ���������� - F3 (������), Shift+F3 (�����)
* ������ ����� ����� ��������� ������� ������ �����
* ������� �� �������� ������ - Ctrl+Alt+C

��������:
� ������� ������������ ������� �� COMMON.lua (EditorMarkText, EditorClearMarks)
-----------------------------------------------
��� ����������� �������� � ���� ���� .properties ��������� ������:
    command.name.130.*=Find String/Word
    command.130.*=dofile $(SciteDefaultHome)\tools\FindText.lua
    command.mode.130.*=subsystem:lua,savebefore:no
    command.shortcut.130.*=Ctrl+Alt+F

    command.name.131.*=Clear All Marks
    command.131.*=dostring EditorClearMarks(0, editor.Length) props["current_mark_style"] = 27
    command.mode.131.*=subsystem:lua,savebefore:no
    command.shortcut.131.*=Ctrl+Alt+C

������������� ���������� ������ � ����� �������� ����� ������������ �������� (� ���� ������� ������������ 5 ��������):
    find.mark.27=#CC00FF
    find.mark.28=#0000FF
    find.mark.29=#00CC66
    find.mark.30=#CCCC00
    find.mark.31=#336600
--]]----------------------------------------------------

local sText = props['CurrentSelection']
local flag = 0
if (sText == '') then
	sText = props['CurrentWord']
	flag = SCFIND_WHOLEWORD
end
local current_mark_style = tonumber(props["current_mark_style"])
if current_mark_style == nil then current_mark_style = 27 end
if current_mark_style < 27 then current_mark_style = 27 end
if string.len(sText) > 0 then
	if flag == SCFIND_WHOLEWORD then
		print('> ����� �������� �����: "'..sText..'"')
	else
		print('> ����� ����������� ������: "'..sText..'"')
	end
	local s,e = editor:findtext(sText,flag,1)
	local count = 0
	if(s~=nil)then
		local m = editor:LineFromPosition(s) - 1
		while s do
			local l = editor:LineFromPosition(s)
			EditorMarkText(s, e-s, current_mark_style)
			count = count + 1
			if l ~= m then
				local str = string.gsub(' '..editor:GetLine(l),'%s+',' ')
				print(props['FileNameExt']..':'..(l + 1)..':\t'..str)
				m = l
			end
			s,e = editor:findtext(sText,flag,e+1)
		end
		print('> �������: '..count..' ���������\nF3 (Shift+F3) - ������� �� ��������\nF4 (Shift+F4) - ������� �� �������\nCtrl+Alt+C - ������� ���� ��������')
	else
		print('> ��������� ['..sText..'] �� �������!')
	end
	current_mark_style = current_mark_style + 1
	if current_mark_style > 31 then current_mark_style = 27 end
	props["current_mark_style"] = current_mark_style
		-- ������������ ����������� �������� �� ���������� � ������� F3 (Shift+F3)
		if flag == SCFIND_WHOLEWORD then
			editor:GotoPos(editor:WordStartPosition(editor.CurrentPos))
		else
			editor:GotoPos(editor.SelectionStart)
		end
		scite.Perform('find:'..sText)
else
	EditorClearMarks(0, editor.Length)
	props["current_mark_style"] = 27
	print('> ������� �������� � ��������� �����, ������� ���������� �����! (����� ������)\n> ����� ������ ���������� ������ �� ������ ����� (����� �����)\n> ��� �� ����� �������� ����� � ���� �������')
end
--~ editor:CharRight() editor:CharLeft() --������� ��������� � ��������� ������
