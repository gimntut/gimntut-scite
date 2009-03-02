--[[--------------------------------------------------
CodePage.lua
Authors: YuriNB, VladVRO, mozers�
Version: 1.2
------------------------------------------------------
������ 2� ��������:
win1251 to cp866 keyboard mapper (YuriNB icq#2614215)
 ������������� ��������� �������� ����� � ����������� win1251/dos866
 ������������ ������������� ��������� ���� �������
�
codepage.lua (VladVRO)
 ����� ������� ��������� � ��������� ������.
------------------------------------------------------
Connection:
 In file SciTEStartup.lua add a line:
    dofile (props["SciteDefaultHome"].."\\tools\\CodePage.lua")
 Set in a file .properties:
    command.name.29.*=DOS Mode (cp866)
    command.29.*=change_codepage_ru
    command.checked.29.*=$(code.page.866)
    command.mode.29.*=subsystem:lua,savebefore:no
--]]--------------------------------------------------

local function UpdateToolBar() -- ������� ������� �������� ������ �� ����� :(
	scite.MenuCommand(IDM_TOGGLEOUTPUT)
	scite.MenuCommand(IDM_TOGGLEOUTPUT)
end

local function UpdateStatusCodePage(mode)
	props["code.page.866"]='0'
	if mode == IDM_ENCODING_UCS2BE then
		props["code.page.name"]='UCS-2 BE'
	elseif mode == IDM_ENCODING_UCS2LE then
		props["code.page.name"]='UCS-2 LE'
	elseif mode == IDM_ENCODING_UTF8 then
		props["code.page.name"]='UTF-8 BOM'
	elseif mode == IDM_ENCODING_UCOOKIE then
		props["code.page.name"]='UTF-8'
	else
		if props["character.set"]=='255' then
			props["code.page.name"]='DOS-866'
			props["code.page.866"]='1'
		elseif props["character.set"]=='204' then
			props["code.page.name"]='WIN-1251'
		elseif tonumber(props["character.set"])==0 then
			props["code.page.name"]='CP1252'
		elseif props["character.set"]=='238' then
			props["code.page.name"]='CP1250'
		elseif props["character.set"]=='161' then
			props["code.page.name"]='CP1253'
		elseif props["character.set"]=='162' then
			props["code.page.name"]='CP1254'
		else
			props["code.page.name"]='???'
		end
	end
	UpdateToolBar()
	scite.UpdateStatusBar()
end

-- ��������� ���� ���������� ������� OnSwitchFile
local old_OnSwitchFile = OnSwitchFile
function OnSwitchFile(file)
	local result
	if old_OnSwitchFile then result = old_OnSwitchFile(file) end
	UpdateStatusCodePage(tonumber(props["editor.unicode.mode"]))
	return result
end

-- ��������� ���� ���������� ������� OnOpen
local old_OnOpen = OnOpen
function OnOpen(file)
	local result
	if old_OnOpen then result = old_OnOpen(file) end
	UpdateStatusCodePage(tonumber(props["editor.unicode.mode"]))
	return result
end

-- ��������� ���� ���������� ������� OnMenuCommand
local old_OnMenuCommand = OnMenuCommand
function OnMenuCommand(cmd, source)
	local result
	if old_OnMenuCommand then result = old_OnMenuCommand(cmd, source) end
	if cmd > 149 and cmd < 155 then -- IDM_ENCODING_DEFAULT, IDM_ENCODING_UCS2BE, IDM_ENCODING_UCS2LE, IDM_ENCODING_UTF8, IDM_ENCODING_UCOOKIE
		UpdateStatusCodePage(cmd)
	end
	return result
end

-------------------------------------------------------------
-- win1251 to cp866 keyboard mapper
-------------------------------------------------------------

function change_codepage_ru()
	scite.MenuCommand(IDM_ENCODING_DEFAULT)
	if props["character.set"]=='255' then
		props["character.set"]='204'
	else
		props["character.set"]='255'
	end
	UpdateStatusCodePage()
end

local charset1251to866 =
{
[168]=240, --�
[184]=241, --�
[185]=252, --�
[192]=128,[193]=129,[194]=130,[195]=131,[196]=132,
[197]=133,[198]=134,[199]=135,[200]=136,[201]=137,
[202]=138,[203]=139,[204]=140,[205]=141,[206]=142,
[207]=143,[208]=144,[209]=145,[210]=146,[211]=147,
[212]=148,[213]=149,[214]=150,[215]=151,[216]=152,
[217]=153,[218]=154,[219]=155,[220]=156,[221]=157,
[222]=158,[223]=159,[224]=160,[225]=161,[226]=162,
[227]=163,[228]=164,[229]=165,[230]=166,[231]=167,
[232]=168,[233]=169,[234]=170,[235]=171,[236]=172,
[237]=173,[238]=174,[239]=175,[240]=224,[241]=225,
[242]=226,[243]=227,[244]=228,[245]=229,[246]=230,
[247]=231,[248]=232,[249]=233,[250]=234,[251]=235,
[252]=236,[253]=237,[254]=238,[255]=239
}

local function Win2DOS(charAdded)
	local a1=string.byte(charAdded)
	if charset1251to866[a1] ~= nil then
		local pos = editor.CurrentPos
		editor:SetSel(pos, pos - 1)
		editor:ReplaceSel( string.char( charset1251to866[a1] ) )
	end
end

-- ��������� ���� ���������� ������� OnChar
local old_OnChar = OnChar
function OnChar(char)
	local result
	if old_OnChar then result = old_OnChar(char) end
	if props["character.set"]=='255' then
		Win2DOS(char)
	end
	return result
end