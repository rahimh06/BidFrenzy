#include "auction.h"

int g_log_pipe_write = -1;
volatile sig_atomic_t g_running = 1;
volatile sig_atomic_t g_outbid = 0;

//tracking child processes to prevent hanging
pid_t g_active_bots[100];
int g_bot_count = 0;

//signal handlers
void handle_sigalrm(int sig)
{
    (void)sig;
    g_running = 0;
}

void handle_sigint(int sig) {
    (void)sig;
    g_running = 0; 
    endwin(); //Close the GUI
    printf("\n[System]: Emergency Shutdown. Cleaning up resources...\n");
    ipc_cleanup(); //Clean up shared memory so the next run isn't bugged
    exit(0);
}

void handle_sigusr1(int sig)
{
    (void)sig;
    g_outbid = 20;
}

void handle_sigchld(int sig)
{
    (void)sig;
    //clear zombie processes silently
    while(waitpid(-1, NULL, WNOHANG) > 0)
    {
        
    }
}

void parse_cmd(char *raw, Command *cmd)
{
    cmd->argc = 0;
    char *tok = strtok(raw, " \n");
    while(tok && cmd->argc < MAX_ARGS)
    {
        strncpy(cmd->argv[cmd->argc++], tok, MAX_ARG_LEN - 1);
        tok = strtok(NULL, " \n");
    }
}

typedef struct
{
    int x;
    int y;
    char text[64];
    int active;
}FloatText;

int main()
{
    srand(time(NULL));
    
    if(ipc_init(100.0) != 0)
    {
        return 1;
    }

    int pipe_fds[2];
    pipe(pipe_fds);
    g_log_pipe_write = pipe_fds[1];
    spawn_logger(pipe_fds[0]);

    signal(SIGALRM, handle_sigalrm);
    signal(SIGINT, handle_sigint);
    signal(SIGUSR1, handle_sigusr1);
    
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    //setup ncurses GUI and colors
    initscr();
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    cbreak();
    noecho();
    nodelay(stdscr, FALSE);
    keypad(stdscr, TRUE);
    curs_set(0);

    int program_active = 1;

    //MASTER SESSION LOOP
    while(program_active)
    {
        int in_menu = 1;
        nodelay(stdscr, FALSE);

        //startup menu loop
        while(in_menu)
        {
            erase();
            attron(COLOR_PAIR(3) | A_BOLD);
            mvprintw(2, 10, "  ____  _     _ _____                              ");
            mvprintw(3, 10, " |  _ \\(_)   | |  ___|                             ");
            mvprintw(4, 10, " | |_) |_  __| | |_ _ __ ___ _ __  _____   _       ");
            mvprintw(5, 10, " |  _ <| |/ _` |  _| '__/ _ \\ '_ \\|__  / | | |     ");
            mvprintw(6, 10, " | |_) | | (_| | | | | |  __/ | | | / /| |_| |     ");
            mvprintw(7, 10, " |____/|_|\\__,_\\_| |_|  \\___|_| |_|/___|\\__, |     ");
            mvprintw(8, 10, "                                         __/ |     ");
            mvprintw(9, 10, "                                        |___/      ");
            attroff(COLOR_PAIR(3) | A_BOLD);

            //Shifted the menu text down to accommodate the bigger logo
            mvprintw(12, 20, "1 - Start Auction");
            mvprintw(13, 20, "2 - Exit Platform");
            mvprintw(15, 15, "Select an option:");
            refresh();

            int c = getch();
            if(c == '1')
            {
                in_menu = 0;
            }
            else if(c == '2')
            {
                program_active = 0;
                in_menu = 0;
            }
        }

        if(program_active == 0)
        {
            break;
        }

        //prepare variables for a fresh auction
        g_bot_count = 0;
        g_running = 1;
        g_shm->auction.highest_bid = 100.0;
        g_shm->auction.winning_pid = 0;
        g_shm->auction.total_bids = 0;
        g_shm->auction.active_bidders = 0;
        
        erase();
        echo();
        curs_set(1);
        mvprintw(10, 10, "Enter the product name for this auction:");
        refresh();
        char p_name[64];
        getnstr(p_name, 63);
        strncpy(g_shm->auction.product_name, p_name, 63);
        
        //reset settings for the main dashboard
        noecho();
        curs_set(0);
        nodelay(stdscr, TRUE);

        g_shm->auction.status = AUCTION_ACTIVE;
        spawn_timer(AUCTION_DURATION);

        char input_buf[MAX_CMD_LEN] = {0};
        int input_pos = 0;
        char status_msg[128] = "System ready. Type 'help' for commands.";
        char help_msg[256] = "";
        time_t start_time = time(NULL);

        FloatText f_bids[5] = {0};
        int last_total_bids = 0;

        //main loop for the auction interface
        while(g_running)
        {
            erase();
            box(stdscr, 0, 0);

            int time_left = AUCTION_DURATION - (time(NULL) - start_time);
            if(time_left < 0)
            {
                time_left = 0;
            }

            attron(COLOR_PAIR(1) | A_BOLD);
            mvprintw(1, 2, " LIVE AUCTION DASHBOARD ");
            attroff(COLOR_PAIR(1) | A_BOLD);

            attron(COLOR_PAIR(4));
            mvprintw(3, 4, "Item on Block : %s", g_shm->auction.product_name);
            attroff(COLOR_PAIR(4));

            mvprintw(5, 4, "Highest Bid   : $%.2f", g_shm->auction.highest_bid);
            mvprintw(6, 4, "Winner PID    : %d", g_shm->auction.winning_pid);
            mvprintw(7, 4, "Active Bots   : %d", g_shm->auction.active_bidders);
            mvprintw(8, 4, "Total Bids    : %d", g_shm->auction.total_bids);
            
            attron(COLOR_PAIR(3));
            mvprintw(10, 4, "Your User PID : %d", getpid());
            attroff(COLOR_PAIR(3));

            //flash red if time is running out
            if(time_left <= 10)
            {
                attron(COLOR_PAIR(2) | A_BLINK);
            }
            mvprintw(5, 40, "TIME REMAINING: %02d seconds", time_left);
            if(time_left <= 10)
            {
                attroff(COLOR_PAIR(2) | A_BLINK);
            }

            if(g_outbid > 0) 
            {
            	attron(COLOR_PAIR(2) | A_BOLD);
	   	mvprintw(11, 4, ">> WARNING: YOU HAVE BEEN OUTBID! <<");
	    	attroff(COLOR_PAIR(2) | A_BOLD);
	    
	    	g_outbid-=1; // Decrement every loop (50ms) until it hits 0
	    }
            //show help box if requested
            if(strlen(help_msg) > 0)
            {
                mvprintw(8, 40, "AVAILABLE COMMANDS:");
                mvprintw(9, 40, "bid <amount>  - Place manual bid");
                mvprintw(10, 40, "status        - Show leader");
                mvprintw(11, 40, "spawn <count> - Create auto-bots");
                mvprintw(12, 40, "exit          - Force stop auction");
            }

            //spawn floating text if someone bid
            if(g_shm->auction.total_bids > last_total_bids)
            {
                for(int i=0; i<5; i++)
                {
                    if(f_bids[i].active == 0)
                    {
                        f_bids[i].active = 1;
                        f_bids[i].x = 60;
                        f_bids[i].y = 4 + (rand() % 6);
                        snprintf(f_bids[i].text, 63, "Bid: $%.2f", g_shm->auction.highest_bid);
                        break;
                    }
                }
                last_total_bids = g_shm->auction.total_bids;
            }

            //draw and move the floating bids
            for(int i=0; i<5; i++)
            {
                if(f_bids[i].active)
                {
                    attron(COLOR_PAIR(3));
                    mvprintw(f_bids[i].y, f_bids[i].x, "%s", f_bids[i].text);
                    attroff(COLOR_PAIR(3));
                    f_bids[i].x--;
                    if(f_bids[i].x < 2)
                    {
                        f_bids[i].active = 0;
                    }
                }
            }

            mvprintw(14, 2, "--------------------------------------------------");
            mvprintw(16, 4, "Status: %s", status_msg);
            mvprintw(18, 4, "Auctioneer_Shell> %s_", input_buf);

            refresh();

            int ch = getch();
            if(ch == '\n')
            {
                Command cmd;
                parse_cmd(input_buf, &cmd);
                memset(help_msg, 0, sizeof(help_msg)); //clear help box on new command
                
                if(cmd.argc > 0)
                {
                    if(strcmp(cmd.argv[0], "exit") == 0)
                    {
                        g_running = 0;
                    }
                    else if(strcmp(cmd.argv[0], "help") == 0)
                    {
                        snprintf(help_msg, 255, "show");
                    }
                    else if(strcmp(cmd.argv[0], "status") == 0)
                    {
                        snprintf(status_msg, 127, "Leader: PID %d with $%.2f", g_shm->auction.winning_pid, g_shm->auction.highest_bid);
                    }
                    else if(strcmp(cmd.argv[0], "bid") == 0 && cmd.argc == 2)
                    {
                        double amt = atof(cmd.argv[1]);
                        
                        //matching proposal output exactly
                        log_event(g_log_pipe_write, "[System]: Checking Semaphore... Lock Acquired.");
                        log_event(g_log_pipe_write, "[System]: Comparing $%.2f to current highest $%.2f...", amt, g_shm->auction.highest_bid);
                        sem_wait(g_sem_vip);//making user the VIP
                        sem_wait(g_sem_bid_lock);
                        if(amt > g_shm->auction.highest_bid)
                        {
                            if(g_shm->auction.winning_pid > 0)
                            {
                                kill(g_shm->auction.winning_pid, SIGUSR1);
                            }
                            g_shm->auction.highest_bid = amt;
                            g_shm->auction.winning_pid = getpid();
                            g_shm->auction.total_bids++;
                            snprintf(status_msg, 127, "[Success]: Bid Accepted!");
                            log_event(g_log_pipe_write, "[Success]: Bid Accepted! New Leader: User (PID: %d)", getpid());
                        }
                        else
                        {
                            snprintf(status_msg, 127, "Bid rejected. Too low.");
                        }
                        sem_post(g_sem_bid_lock);
                        sem_post(g_sem_vip);
                        log_event(g_log_pipe_write, "[System]: Releasing Semaphore.");
                    }
                    else if(strcmp(cmd.argv[0], "spawn") == 0 && cmd.argc == 2)
                    {
                        int count = atoi(cmd.argv[1]);
                        for(int i=0; i<count; i++)
                        {
                            if(g_bot_count < 100)
                            {
                                double random_budget = (double)(rand() % 100000) + 100.0;
                                g_active_bots[g_bot_count] = spawn_bidder(0, random_budget);
                                g_bot_count++;
                            }
                        }
                        snprintf(status_msg, 127, "Spawned %d bots with random budgets.", count);
                    }
                }
                memset(input_buf, 0, MAX_CMD_LEN);
                input_pos = 0;
            }
            else if(ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
            {
                if(input_pos > 0)
                {
                    input_buf[--input_pos] = '\0';
                }
            }
            else if(ch != ERR && input_pos < MAX_CMD_LEN - 1 && ch >= 32 && ch <= 126)
            {
                input_buf[input_pos++] = ch;
            }
            
            napms(100); //UI tick
        }

        //Auction End Sequence
        g_shm->auction.status = AUCTION_SOLD;
        
        //proposal exact logs
        log_event(g_log_pipe_write, "[SIGNAL]: SIGALRM Received.");
        log_event(g_log_pipe_write, "AUCTION CLOSED: SOLD!");
        log_event(g_log_pipe_write, "Final Price: $%.2f", g_shm->auction.highest_bid);
        log_event(g_log_pipe_write, "Winner PID: %d won item '%s'", g_shm->auction.winning_pid, g_shm->auction.product_name);
        log_event(g_log_pipe_write, "Total Bids Processed: %d", g_shm->auction.total_bids);

        //Give bots time to notice the auction ended and exit gracefully
        log_event(g_log_pipe_write, "Waiting for bidders to exit naturally...");
        sleep(2); //bots check status every 2-4 seconds, so give them time

        //Now wait() for each bot to terminate properly
        for(int i=0; i<g_bot_count; i++)
        {
            if(g_active_bots[i] > 0)
            {
                int status;
                pid_t result = waitpid(g_active_bots[i], &status, WNOHANG);
                
                //if still running after grace period, then kill it
                if(result == 0)
                {
                    kill(g_active_bots[i], SIGTERM);
                    waitpid(g_active_bots[i], NULL, 0); //now block until dead
                }
            }
        }

        //timer doesnt check auction status so we still kill it
        if(g_shm->timer_pid > 0)
        {
            kill(g_shm->timer_pid, SIGTERM);
            waitpid(g_shm->timer_pid, NULL, 0);
        }

        log_event(g_log_pipe_write, "Cleaning up child processes... Done");
        log_event(g_log_pipe_write, "Audit Log saved to: history.log");

        //Draw final result box over the screen
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(8, 20, "*************************************");
        mvprintw(9, 20, "* AUCTION CLOSED: SOLD!       *");
        mvprintw(10, 20, "* Final Price: $%-19.2f *", g_shm->auction.highest_bid);
        mvprintw(11, 20, "* Winner PID : %-20d *", g_shm->auction.winning_pid);
        mvprintw(12, 20, "*************************************");
        attroff(COLOR_PAIR(4) | A_BOLD);
        
        mvprintw(14, 25, "Press any key to continue...");
        refresh();
        
        nodelay(stdscr, FALSE);
        getch(); //wait for user before going back to menu
    }

    //Completely exit the platform
    endwin();
    printf("\033[H\033[J");//clear screen so we can see terminal
    close(g_log_pipe_write);
    ipc_cleanup();
    printf("Platform exited clean. Check history.log for details.\n");
    return 0;
}
