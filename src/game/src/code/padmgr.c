#include "global.h"
#include "vt.h"
#include <string.h>
#include "resources/ResourceManagerHelpers.h"

int32_t D_8012D280 = 1;

OSMesgQueue* PadMgr_LockSerialMesgQueue(PadMgr* padMgr) {
    OSMesgQueue* ctrlrQ = NULL;

    if (D_8012D280 > 2) {
        // "serialMsgQ Waiting for lock"
        osSyncPrintf("%2d %d serialMsgQロック待ち         %08x %08x          %08x\n", osGetThreadId(NULL),
                     padMgr->serialMsgQ.validCount, padMgr, &padMgr->serialMsgQ, &ctrlrQ);
    }

    osRecvMesg(&padMgr->serialMsgQ, (OSMesg*)&ctrlrQ, OS_MESG_BLOCK);

    if (D_8012D280 > 2) {
        // "serialMsgQ Locked"
        osSyncPrintf("%2d %d serialMsgQをロックしました                     %08x\n", osGetThreadId(NULL),
                     padMgr->serialMsgQ.validCount, ctrlrQ);
    }

    return ctrlrQ;
}

void PadMgr_UnlockSerialMesgQueue(PadMgr* padMgr, OSMesgQueue* ctrlrQ) {
    if (D_8012D280 > 2) {
        // "serialMsgQ Unlock"
        osSyncPrintf("%2d %d serialMsgQロック解除します   %08x %08x %08x\n", osGetThreadId(NULL),
                     padMgr->serialMsgQ.validCount, padMgr, &padMgr->serialMsgQ, ctrlrQ);
    }

    osSendMesgPtr(&padMgr->serialMsgQ, ctrlrQ, OS_MESG_BLOCK);

    if (D_8012D280 > 2) {
        // "serialMsgQ Unlocked"
        osSyncPrintf("%2d %d serialMsgQロック解除しました %08x %08x %08x\n", osGetThreadId(NULL),
                     padMgr->serialMsgQ.validCount, padMgr, &padMgr->serialMsgQ, ctrlrQ);
    }
}

void PadMgr_LockPadData(PadMgr* padMgr) {
    osRecvMesg(&padMgr->lockMsgQ, NULL, OS_MESG_BLOCK);
}

void PadMgr_UnlockPadData(PadMgr* padMgr) {
    osSendMesgPtr(&padMgr->lockMsgQ, NULL, OS_MESG_BLOCK);
}

void PadMgr_RumbleControl(PadMgr* padMgr) {
    static uint32_t errcnt = 0;
    static uint32_t frame;
    int32_t temp = 1;
    OSMesgQueue* ctrlrQ = PadMgr_LockSerialMesgQueue(padMgr);
    int32_t i;

    int32_t triedRumbleComm = 0;

    for (i = 0; i < 4; i++) {
        if (padMgr->ctrlrIsConnected[i]) {
            if (padMgr->padStatus[i].status & 1) {
                if (padMgr->pakType[i] == temp) {
                    if (padMgr->rumbleEnable[i] != 0) {
                        if (padMgr->rumbleCounter[i] < 3) {
                            // clang-format off
                            osSyncPrintf(VT_FGCOL(YELLOW));
                            // clang-format on

                            // "Vibration pack jumble jumble"?
                            osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "振動パック ぶるぶるぶるぶる");
                            osSyncPrintf(VT_RST);

                            if (__osMotorAccess(&padMgr->pfs[i], temp) != 0) {
                                padMgr->pakType[i] = 0;
                                osSyncPrintf(VT_FGCOL(YELLOW));
                                // "A communication error has occurred with the vibration pack"
                                osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "振動パックで通信エラーが発生しました");
                                osSyncPrintf(VT_RST);
                            } else {
                                padMgr->rumbleCounter[i] = 3;
                            }

                            triedRumbleComm = 1;
                        }
                    } else {
                        if (padMgr->rumbleCounter[i] != 0) {
                            // clang-format off
                            osSyncPrintf(VT_FGCOL(YELLOW));
                            // clang-format on

                            // "Stop vibration pack"
                            osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "振動パック 停止");
                            osSyncPrintf(VT_RST);

                            if (osMotorStop(&padMgr->pfs[i]) != 0) {
                                padMgr->pakType[i] = 0;
                                osSyncPrintf(VT_FGCOL(YELLOW));
                                // "A communication error has occurred with the vibration pack"
                                osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "振動パックで通信エラーが発生しました");
                                osSyncPrintf(VT_RST);
                            } else {
                                padMgr->rumbleCounter[i]--;
                            }

                            triedRumbleComm = 1;
                        }
                    }
                }
            } else {
                if (padMgr->pakType[i] != 0) {
                    if (padMgr->pakType[i] == 1) {
                        osSyncPrintf(VT_FGCOL(YELLOW));
                        // "It seems that a vibration pack was pulled out"
                        osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "振動パックが抜かれたようです");
                        osSyncPrintf(VT_RST);
                        padMgr->pakType[i] = 0;
                    } else {
                        osSyncPrintf(VT_FGCOL(YELLOW));
                        // "It seems that a controller pack that is not a vibration pack was pulled out"
                        osSyncPrintf("padmgr: %dコン: %s\n", i + 1,
                                     "振動パックではないコントローラパックが抜かれたようです");
                        osSyncPrintf(VT_RST);
                        padMgr->pakType[i] = 0;
                    }
                }
            }
        }
    }

    if (!triedRumbleComm) {
        i = frame % 4;

        if (padMgr->ctrlrIsConnected[i] && (padMgr->padStatus[i].status & 1) && (padMgr->pakType[i] != 1)) {
            int32_t var4 = osMotorInit(ctrlrQ, &padMgr->pfs[i], i);

            if (var4 == 0) {
                padMgr->pakType[i] = 1;
                osMotorStart(&padMgr->pfs[i]);
                osMotorStop(&padMgr->pfs[i]);
                osSyncPrintf(VT_FGCOL(YELLOW));
                // "Recognized vibration pack"
                osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "振動パックを認識しました");
                osSyncPrintf(VT_RST);
            } else if (var4 == 11) {
                padMgr->pakType[i] = 2;
            } else if (var4 == 4) {
                LOG_NUM("++errcnt", ++errcnt);
                osSyncPrintf(VT_FGCOL(YELLOW));
                // "Controller pack communication error"
                osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "コントローラパックの通信エラー");
                osSyncPrintf(VT_RST);
            }
        }
    }

    frame++;
    PadMgr_UnlockSerialMesgQueue(padMgr, ctrlrQ);
}

void PadMgr_RumbleStop(PadMgr* padMgr) {
    int32_t i;
    OSMesgQueue* ctrlrQ = PadMgr_LockSerialMesgQueue(padMgr);

    for (i = 0; i < 4; i++) {
        if (osMotorInit(ctrlrQ, &padMgr->pfs[i], i) == 0) {

            osMotorStop(&padMgr->pfs[i]);
        }
    }

    PadMgr_UnlockSerialMesgQueue(padMgr, ctrlrQ);
}

void PadMgr_RumbleReset(PadMgr* padMgr) {
    padMgr->rumbleOffFrames = 3;
}

void PadMgr_RumbleSetSingle(PadMgr* padMgr, uint32_t ctrlr, uint32_t rumble) {
    padMgr->rumbleEnable[ctrlr] = rumble;
    padMgr->rumbleOnFrames = 240;
}

void PadMgr_RumbleSet(PadMgr* padMgr, uint8_t* ctrlrRumbles) {
    int32_t i;

    for (i = 0; i < 4; i++) {
        padMgr->rumbleEnable[i] = ctrlrRumbles[i];
    }

    padMgr->rumbleOnFrames = 240;
}

#define PAUSE_BUFFER_INPUT_BLOCK_ID 0
void PadMgr_ProcessInputs(PadMgr* padMgr) {
    int32_t i;

    PadMgr_LockPadData(padMgr);

    ControllerInput* input = &padMgr->inputs[0];
    OSContPad* padnow1 = &padMgr->pads[0];

    for (i = 0; i < padMgr->nControllers; i++, input++, padnow1++) {
        input->prev = input->cur;

        switch (padnow1->err_no) {
            case 0:
                input->cur = *padnow1;

                if (!padMgr->ctrlrIsConnected[i]) {
                    padMgr->ctrlrIsConnected[i] = true;
                    osSyncPrintf(VT_FGCOL(YELLOW));
                    osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "認識しました"); // "Recognized"
                    osSyncPrintf(VT_RST);
                }
                break;
            case 4:
                input->cur = input->prev;
                LOG_NUM("this->Key_switch[i]", padMgr->ctrlrIsConnected[i]);
                osSyncPrintf(VT_FGCOL(YELLOW));
                // "Overrun error occurred"
                osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "オーバーランエラーが発生");
                osSyncPrintf(VT_RST);
                break;
            case 8:
                input->cur.button = 0;
                input->cur.stick_x = 0;
                input->cur.stick_y = 0;
                input->cur.err_no = padnow1->err_no;
                if (padMgr->ctrlrIsConnected[i]) {
                    padMgr->ctrlrIsConnected[i] = false;
                    padMgr->pakType[i] = 0;
                    padMgr->rumbleCounter[i] = 0xFF;
                    osSyncPrintf(VT_FGCOL(YELLOW));
                    // "Do not respond"?
                    osSyncPrintf("padmgr: %dコン: %s\n", i + 1, "応答しません");
                    osSyncPrintf(VT_RST);
                }
                break;
            default:
                LOG_HEX("padnow1->errno", padnow1->err_no);
                Fault_AddHungupAndCrash(__FILE__, __LINE__);
        }

        int32_t buttonDiff = input->prev.button ^ input->cur.button;
        input->press.button |= (uint16_t)(buttonDiff & input->cur.button);
        input->rel.button |= (uint16_t)(buttonDiff & input->prev.button);
        PadUtils_UpdateRelXY(input);
        input->press.stick_x += (int8_t)(input->cur.stick_x - input->prev.stick_x);
        input->press.stick_y += (int8_t)(input->cur.stick_y - input->prev.stick_y);
        // PC controller right-stick state.
        PadUtils_UpdateRelRXY(input);
        input->press.right_stick_x += (int8_t)(input->cur.right_stick_x - input->prev.right_stick_x);
        input->press.right_stick_y += (int8_t)(input->cur.right_stick_y - input->prev.right_stick_y);
    }

    PadMgr_UnlockPadData(padMgr);
}

void PadMgr_HandleRetraceMsg(PadMgr* padMgr) {
    int32_t i;
    OSMesgQueue* queue = PadMgr_LockSerialMesgQueue(padMgr);
    uint32_t mask = { 0 };

    osContStartReadData(queue);
    if (padMgr->retraceCallback) {
        padMgr->retraceCallback(padMgr, padMgr->retraceCallbackValue);
    }
    osRecvMesg(queue, NULL, OS_MESG_BLOCK);
    osContGetReadData(padMgr->pads);


    for (i = 0; i < __osMaxControllers; i++) {
        padMgr->padStatus[i].status = 0;
    }

    if (padMgr->preNMIShutdown) {
        memset(padMgr->pads, 0, sizeof(padMgr->pads));
    }
    PadMgr_ProcessInputs(padMgr);
    osContStartQuery(queue);
    osRecvMesg(queue, NULL, OS_MESG_BLOCK);
    osContGetQuery(padMgr->padStatus);
    PadMgr_UnlockSerialMesgQueue(padMgr, queue);

    mask = 0;
    for (i = 0; i < 4; i++) {
        if (padMgr->padStatus[i].err_no == 0) {
            if (padMgr->padStatus[i].type == CONT_TYPE_NORMAL) {
                mask |= 1 << i;
            } else {
                // LOG_HEX("this->pad_status[i].type", padMgr->padStatus[i].type);
                //  "An unknown type of controller is connected"
                // osSyncPrintf("知らない種類のコントローラが接続されています\n");
            }
        }
    }
    padMgr->validCtrlrsMask = mask;

    if (padMgr->rumbleOffFrames > 0) {
        --padMgr->rumbleOffFrames;
        PadMgr_RumbleStop(padMgr);
    } else if (padMgr->rumbleOnFrames == 0) {
        PadMgr_RumbleStop(padMgr);
    } else if (!padMgr->preNMIShutdown) {
        PadMgr_RumbleControl(padMgr);
        --padMgr->rumbleOnFrames;
    }
}

void PadMgr_HandlePreNMI(PadMgr* padMgr) {
    osSyncPrintf("padmgr_HandlePreNMI()\n");
    padMgr->preNMIShutdown = true;
    PadMgr_RumbleReset(padMgr);
}

void PadMgr_RequestPadData(PadMgr* padMgr, ControllerInput* inputs, int32_t mode) {
    int32_t i;

    PadMgr_LockPadData(padMgr);

    ControllerInput* ogInput = &padMgr->inputs[0];
    ControllerInput* newInput = &inputs[0];
    for (i = 0; i < 4; i++) {
        if (mode != 0) {
            *newInput = *ogInput;
            ogInput->press.button = 0;
            ogInput->press.stick_x = 0;
            ogInput->press.stick_y = 0;
            ogInput->rel.button = 0;
        } else {
            newInput->prev = newInput->cur;
            newInput->cur = ogInput->cur;
            int32_t buttonDiff = newInput->prev.button ^ newInput->cur.button;
            newInput->press.button = newInput->cur.button & buttonDiff;
            newInput->rel.button = newInput->prev.button & buttonDiff;
            PadUtils_UpdateRelXY(newInput);
            newInput->press.stick_x += (int8_t)(newInput->cur.stick_x - newInput->prev.stick_x);
            newInput->press.stick_y += (int8_t)(newInput->cur.stick_y - newInput->prev.stick_y);
            // PC controller right-stick state.
            PadUtils_UpdateRelRXY(newInput);
            newInput->press.right_stick_x += (int8_t)(newInput->cur.right_stick_x - newInput->prev.right_stick_x);
            newInput->press.right_stick_y += (int8_t)(newInput->cur.right_stick_y - newInput->prev.right_stick_y);
        }
        ogInput++;
        newInput++;
    }

    PadMgr_UnlockPadData(padMgr);
}

void PadMgr_ThreadEntry(PadMgr* padMgr) {
    int16_t* mesg = NULL;

    // osSyncPrintf("コントローラスレッド実行開始\n"); // "Controller thread execution start"

    int32_t exit = false;
    while (!exit) {
        if ((D_8012D280 > 2) && (padMgr->interruptMsgQ.validCount == 0)) {
            // "Waiting for controller thread event"
            osSyncPrintf("コントローラスレッドイベント待ち %lld\n", OS_CYCLES_TO_USEC(osGetTime()));
        }

        osRecvMesg(&padMgr->interruptMsgQ, (OSMesg*)&mesg, OS_MESG_BLOCK);
        // LOG_CHECK_NULL_POINTER("msg", mesg);

        PadMgr_HandleRetraceMsg(padMgr);
        break;

#if 0
        switch (*mesg) {
            case OS_SC_RETRACE_MSG:
                if (D_8012D280 > 2) {
                    osSyncPrintf("padmgr_HandleRetraceMsg START %lld\n", OS_CYCLES_TO_USEC(osGetTime()));
                }

                PadMgr_HandleRetraceMsg(padMgr);

                if (D_8012D280 > 2) {
                    osSyncPrintf("padmgr_HandleRetraceMsg END   %lld\n", OS_CYCLES_TO_USEC(osGetTime()));
                }

                break;
            case OS_SC_PRE_NMI_MSG:
                PadMgr_HandlePreNMI(padMgr);
                break;
            case OS_SC_NMI_MSG:
                exit = true;
                break;
        }
#endif
    }

    // OTRTODO: Removed due to crash
    // IrqMgr_RemoveClient(padMgr->irqMgr, &padMgr->irqClient);

    // osSyncPrintf("コントローラスレッド実行終了\n"); // "Controller thread execution end"
}

void PadMgr_Init(PadMgr* padMgr, OSMesgQueue* siIntMsgQ, IrqMgr* irqMgr, OSId id, OSPri priority, void* stack) {
    osSyncPrintf("パッドマネージャ作成 padmgr_Create()\n"); // "Pad Manager creation"

    memset(padMgr, 0, sizeof(PadMgr));
    padMgr->irqMgr = irqMgr;

    osCreateMesgQueue(&padMgr->interruptMsgQ, padMgr->interruptMsgBuf, 4);
    // OTRTODO: Removed due to crash
    // IrqMgr_AddClient(padMgr->irqMgr, &padMgr->irqClient, &padMgr->interruptMsgQ);
    osCreateMesgQueue(&padMgr->serialMsgQ, padMgr->serialMsgBuf, 1);
    PadMgr_UnlockSerialMesgQueue(padMgr, siIntMsgQ);
    osCreateMesgQueue(&padMgr->lockMsgQ, padMgr->lockMsgBuf, 1);
    PadMgr_UnlockPadData(padMgr);
    PadSetup_Init(siIntMsgQ, (uint8_t*)&padMgr->validCtrlrsMask, padMgr->padStatus);

    padMgr->nControllers = 4;
    osContSetCh(padMgr->nControllers);

    osCreateThread(&padMgr->thread, id, (void (*)(void*))PadMgr_ThreadEntry, padMgr, stack, priority);
    osStartThread(&padMgr->thread);
}
