ow_0_0_MapScripts::
	.byte 0

ow_0_0_EventScript_PanTest::
        faceplayer
	msgbox Text_PanTestMsg, MSGBOX_YESNO
	compare VAR_RESULT, 0
	goto_if_eq ow_0_0_EventScript_NoPanTest
	pantoentity 6, 20
	msgbox Text_PanWaitMsg, MSGBOX_DEFAULT
	pantoplayer 20
	release
	end

ow_0_0_EventScript_NoPanTest::
        msgbox Text_PanTestDeclined, MSGBOX_DEFAULT
	release
	end

Text_PanTestMsg:
        .string "I'm a cool test NPC. I can test the\n"
	.string "'pan to entity' feature of the camera.\p"
	.string "Pan to the item on the mountain?$"

Text_PanWaitMsg:
        .string "Press a button when you're ready\n"
	.string "to come back!$"

Text_PanTestDeclined:
        .string "Fair enough.$"
