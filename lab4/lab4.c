// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>

#include <stdint.h>
#include <stdio.h>

// Any header files included below this line should have been created by you

#include "mouse.h"

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need/ it]
  // lcf_trace_calls("/home/lcom/labs/lab4/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  // lcf_log_output("/home/lcom/labs/lab4/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}

extern uint16_t byte_packet[3];
extern uint32_t byte;
extern struct packet prPacket;


int (mouse_test_packet)(uint32_t cnt) {
    int ipc_status;
    message msg;
    uint16_t r;
    uint32_t mouseID = 12;
    uint32_t counter = 0;

    if (mouse_issue_cmd(ENABLE) != 0)
    {
        printf("error::mouse_issue_cmd(ENABLE)\n");
        return 1;
    }

    if (mouse_subscribe_int(&mouseID) != 0) {
        return 1;
    }

    uint32_t irq_set = mouseID;
    uint16_t index = 0;

    while (counter < cnt) {
        /* Get a request message. */
        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
            printf("driver_receive failed with: %d", r);
            continue;
        }
        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */
                    if (msg.m_notify.interrupts & irq_set) { /* subscribed interrupt */
                        mouse_ih();

                        if (index == 0 && (byte & CHECK_BYTE1)) {
                            byte_packet[0] = byte;
                            index += 1;
                        }
                        else if (index == 1) {
                            byte_packet[1] = byte;
                            index += 1;
                        }
                        else if (index == 2) {
                            byte_packet[2] = byte;
                            process_packet();
                            mouse_print_packet(&prPacket);
                            index = 0;
                            counter += 1;
                        }
                    }
                    break;
                default:
                    break; /* no other notifications expected: do nothing */
            }
        } else { /* received a standard message, not a notification */
            /* no standard messages expected: do nothing */
        }
    }

    if (mouse_unsubscribe_int() != 0) {
        return 1;
    }

    if (mouse_issue_cmd(DISABLE) != 0)
    {
        printf("error::mouse_issue_cmd(DISABLE)\n");
        return 1;
    }

    return 0;
}

int (mouse_test_remote)(uint16_t period, uint8_t cnt) {

    uint16_t count = 0;
    while (count < cnt) {
        count++;

        mouse_issue_cmd(READ); //ler em remote mode em vez de streaming mode

        process_remote(); //processa o packet em modo remote
        process_packet(); //analisa o packet e dá os valores na struct

        mouse_print_packet(&prPacket);
        tickdelay(micros_to_ticks(period*1000));
    }
    mouse_issue_cmd(SET_STREAM);
    mouse_issue_cmd(DISABLE);

    uint8_t command = minix_get_dflt_kbc_cmd_byte(); //reset kbc

    sys_outb(CMD_REG, WRITE_CMD_BYTE);
    sys_outb(BUF_IN, command);

    return 0;
}

extern uint32_t time_counter;

int (mouse_test_async)(uint8_t idle_time) {
    int ipc_status;
    message msg;
    uint16_t r;
    uint32_t mouseID = 12;
    uint8_t irq_set_timer;

    if (mouse_issue_cmd(ENABLE) != 0)
    {
        printf("error::mouse_issue_cmd(ENABLE)\n");
        return 1;
    }

    if (mouse_subscribe_int(&mouseID) != 0 || timer_subscribe_int(&irq_set_timer) != 0) {
        printf("error::mouse_subscribe_int(&mouseID) or timer_subscribe_int(&timerID)\n");
        return 1;
    }

    uint32_t irq_set = mouseID;

    uint16_t index = 0;

    time_counter = 0;

    while (time_counter < idle_time * 60) {
        /* Get a request message. */
        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
            printf("driver_receive failed with: %d", r);
            continue;
        }
        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */
                    if (msg.m_notify.interrupts & irq_set_timer) {
                        timer_int_handler();
/*                        if (time_counter >= idle_time * 60) {
                            break;
                        }*/
                    }
                    if (msg.m_notify.interrupts & irq_set) { /* subscribed interrupt */
                        mouse_ih();
                        time_counter = 0;
                        if (index == 0 && (byte & CHECK_BYTE1)) {
                            byte_packet[0] = byte;
                            index += 1;
                        } else if (index == 1) {
                            byte_packet[1] = byte;
                            index += 1;
                        } else if (index == 2) {
                            byte_packet[2] = byte;
                            process_packet();
                            mouse_print_packet(&prPacket);
                            index = 0;
                            // time_counter++;
                        }
                        time_counter = 0;
                    }
                    break;
                default:
                    break; /* no other notifications expected: do nothing */
            }
        } else { /* received a standard message, not a notification */
            /* no standard messages expected: do nothing */
        }
    }

    if (mouse_unsubscribe_int() != 0) {
        printf("error::mouse_unsubscribe_int()\n");
        return 1;
    }

    if (timer_unsubscribe_int() != 0) {
        printf("timer_unsubscribe_int()\n");
        return 1;
    }

    if (mouse_issue_cmd(DISABLE) != 0)
    {
        printf("error::mouse_issue_cmd(DISABLE)\n");
        return 1;
    }

    return 0;
}

extern state_t state;

int (mouse_test_gesture)(uint8_t x_len, uint8_t tolerance) {
    int ipc_status;
    message msg;
    uint16_t r;
    uint32_t mouseID = 12;

    if (mouse_issue_cmd(ENABLE) != 0)
    {
        printf("error::mouse_issue_cmd(ENABLE)\n");
        return 1;
    }

    if (mouse_subscribe_int(&mouseID) != 0) {
        return 1;
    }

    uint32_t irq_set = mouseID;
    uint16_t index = 0;

    int32_t x = 0;


    event_t event;

    while (state != C) {
        /* Get a request message. */
        if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
            printf("driver_receive failed with: %d", r);
            continue;
        }
        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */
                    if (msg.m_notify.interrupts & irq_set) { /* subscribed interrupt */
                        mouse_ih();

                        if (index == 0 && (byte & CHECK_BYTE1)) {
                            byte_packet[0] = byte;
                            index += 1;
                        }
                        else if (index == 1) {
                            byte_packet[1] = byte;
                            index += 1;
                        }
                        else if (index == 2) {
                            byte_packet[2] = byte;
                            process_packet();
                            index = 0;

                            x += prPacket.delta_x;

                            if (state == I1) {
                                x = 0;
                                if (prPacket.lb)
                                    event = MOV1;
                                else
                                    event = A1; // just so it doesn't fail
                            }

                            if (state == D1) {
                                if ((prPacket.delta_y + tolerance) / (prPacket.delta_x + tolerance) < 0)
                                    event = A1;
                                if (!prPacket.lb)
                                    event = LB_UP;
                                else if (x >= x_len)
                                    event = B;
                            }

                            if (state == L1) {
                                if ((prPacket.delta_y + tolerance) / (prPacket.delta_x + tolerance) < 0)
                                    event = A1;
                                else if (!prPacket.lb)
                                    event = LB_UP;
                            }

                            if (state == I2)
                            {
                                x = 0;
                                if (prPacket.rb)
                                    event = MOV2;
                                if (prPacket.lb)
                                    event = LB;
                            }

                            if (state == D2) {
                                if ((prPacket.delta_y - tolerance) / (prPacket.delta_x + tolerance) > 0)
                                    event = A2;
                                if (!prPacket.rb)
                                    event = RB_UP;
                                else
                                    event = B;
                            }

                            if (state == L2) {
                                if ((prPacket.delta_y - tolerance) / (prPacket.delta_x + tolerance) > 0)
                                    event = A2;
                                else if (!prPacket.rb)
                                    event = RB_UP;
                            }

                            check_line(&event);
                            mouse_print_packet(&prPacket);
                        }
                    }
                    break;
                default:
                    break; /* no other notifications expected: do nothing */
            }
        } else { /* received a standard message, not a notification */
            /* no standard messages expected: do nothing */
        }
    }

    if (mouse_unsubscribe_int() != 0) {
        return 1;
    }

    if (mouse_issue_cmd(DISABLE) != 0)
    {
        printf("error::mouse_issue_cmd(DISABLE)\n");
        return 1;
    }

    return 0;
}
