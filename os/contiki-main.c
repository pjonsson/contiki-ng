/*
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup main
 * @{
 */
/*---------------------------------------------------------------------------*/
/**
 * \file
 *
 * Implementation of \os's main routine
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "sys/node-id.h"
#include "sys/platform.h"
#include "sys/energest.h"
#include "sys/stack-check.h"
#include "dev/watchdog.h"

#include "net/queuebuf.h"
#include "net/app-layer/coap/coap-engine.h"
#include "net/app-layer/snmp/snmp.h"
#include "services/rpl-border-router/rpl-border-router.h"
#include "services/orchestra/orchestra.h"
#include "services/shell/serial-shell.h"
#include "services/simple-energest/simple-energest.h"
#include "services/tsch-cs/tsch-cs.h"

#include <stdio.h>
#include <stdint.h>
/*---------------------------------------------------------------------------*/
/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "Main"
#define LOG_LEVEL LOG_LEVEL_MAIN

#if PLATFORM_MAIN_ACCEPTS_ARGS
/*---------------------------------------------------------------------------*/
int contiki_argc;
char **contiki_argv;
/*---------------------------------------------------------------------------*/
#include "lib/list.h"

LIST(contiki_options);
int flag_verbose;
static const char *prog;
static const char *help_usage;
static const char *help_suffix;
/*---------------------------------------------------------------------------*/
void
contiki_set_usage(const char *msg)
{
  help_usage = msg;
}
/*---------------------------------------------------------------------------*/
void
contiki_set_extra_help(const char *msg)
{
  help_suffix = msg;
}
/*---------------------------------------------------------------------------*/
void
contiki_add_option(struct contiki_option *option)
{
  static bool initialized = false;
  if(!initialized) {
    list_init(contiki_options);
    initialized = true;
  }
  list_add(contiki_options, option);
}
/*---------------------------------------------------------------------------*/
static void
print_help(void)
{
  printf("usage: %s [options]", prog);
  if(help_usage) {
    printf(" %s", help_usage);
  }
  printf("\n\nOptions are:\n");
  for(struct contiki_option *r = list_head(contiki_options);
      r != NULL; r = r->next) {
    if(!r->help) {
      continue;
    }
    printf("  ");
    if(r->opt_struct.flag == NULL && r->opt_struct.val) {
      printf("-%c, ", (char)r->opt_struct.val);
    }
    const char *short_or_long = strlen(r->opt_struct.name) == 1 ? "" : "-";
    printf("-%s%s", short_or_long, r->opt_struct.name);

    int has_arg = r->opt_struct.has_arg;
    if(has_arg != no_argument) {
      printf(has_arg == optional_argument ? " [%s]" : " <%s>",
             r->arg_name ? r->arg_name : "value");
    }
    printf("\n" CONTIKI_HELP_PREFIX "%s\n", r->help);
  }
  if(help_suffix) {
    printf("%s\n", help_suffix);
  }
}
/*---------------------------------------------------------------------------*/
static int
verbose_callback(const char *optarg)
{
  flag_verbose = optarg ? atoi(optarg) : 3;
  if(flag_verbose < 0 || flag_verbose > 5 ||
     (flag_verbose == 0 && optarg && optarg[0] != '0')) {
    fprintf(stderr, "Verbose level '%s' not between 0 and 5\n", optarg);
    return 1;
  }
  return 0;
}
CONTIKI_OPTION(CONTIKI_VERBOSE_PRIO, {"v", optional_argument, NULL, 0},
               verbose_callback, "verbosity level (0-5)\n", "verbosity");
/*---------------------------------------------------------------------------*/
CC_NORETURN static int
help_callback(const char *optarg)
{
  print_help();
  exit(0);
}
CONTIKI_OPTION(CONTIKI_MAX_INIT_PRIO + 1, {"help", no_argument, NULL, 'h'},
               help_callback, "display this help and exit");
/*---------------------------------------------------------------------------*/
static int
parse_argv(int *argc, char ***argv)
{
  prog = *argv[0];
  const int num_options = list_length(contiki_options);
  struct contiki_option options[num_options];
  struct option long_options[num_options + 1];

  int i = 0;
  for(struct contiki_option *r = list_head(contiki_options);
      r != NULL; ++i, r = r->next) {
    memcpy(&long_options[i], &r->opt_struct, sizeof(struct option));
    memcpy(&options[i], r, sizeof(struct contiki_option));
  }
  /* Null terminate options. */
  memset(&long_options[i], 0, sizeof(struct option));

  while (1) {
    int ix = 0;
    int c = getopt_long_only(*argc, *argv, "", long_options, &ix);
    if(c == -1) { /* Processed all options. */
      break;
    }
    if(c == '?') { /* Unknown option, print help and return error. */
      print_help();
      return 1;
    }
    if(options[ix].callback) { /* Option has a callback, call with optarg. */
      int rv;
      if((rv = options[ix].callback(optarg)) != 0) {
        return rv;
      }
    }
  }
  *argc -= optind - 1;
  *argv += optind - 1;
  return 0;
}
/*---------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
  int rv;
  if((rv = parse_argv(&argc, &argv)) != 0) {
    return rv;
  }
  /* Remember argc/argv after command line options. */
  contiki_argc = argc;
  contiki_argv = argv;
#else
int
main(void)
{
#endif /* PLATFORM_MAIN_ACCEPTS_ARGS */
  platform_init_stage_one();

  clock_init();
  rtimer_init();
  process_init();
  process_start(&etimer_process, NULL);
  ctimer_init();
  watchdog_init();

  energest_init();

#if STACK_CHECK_ENABLED
  stack_check_init();
#endif

  platform_init_stage_two();

#if QUEUEBUF_ENABLED
  queuebuf_init();
#endif /* QUEUEBUF_ENABLED */
  netstack_init();
  node_id_init();

  LOG_INFO("Starting " CONTIKI_VERSION_STRING "\n");
  LOG_DBG("TARGET=%s", CONTIKI_TARGET_STRING);
#ifdef CONTIKI_BOARD_STRING
  LOG_DBG_(", BOARD=%s", CONTIKI_BOARD_STRING);
#endif
  LOG_DBG_("\n");
  LOG_INFO("- Routing: %s\n", NETSTACK_ROUTING.name);
  LOG_INFO("- Net: %s\n", NETSTACK_NETWORK.name);
  LOG_INFO("- MAC: %s\n", NETSTACK_MAC.name);
  LOG_INFO("- 802.15.4 PANID: 0x%04x\n", IEEE802154_PANID);
#if MAC_CONF_WITH_TSCH
  LOG_INFO("- 802.15.4 TSCH default hopping sequence length: %u\n", (unsigned)sizeof(TSCH_DEFAULT_HOPPING_SEQUENCE));
#else /* MAC_CONF_WITH_TSCH */
  LOG_INFO("- 802.15.4 Default channel: %u\n", IEEE802154_DEFAULT_CHANNEL);
#endif /* MAC_CONF_WITH_TSCH */

  LOG_INFO("Node ID: %u\n", node_id);
  LOG_INFO("Link-layer address: ");
  LOG_INFO_LLADDR(&linkaddr_node_addr);
  LOG_INFO_("\n");

#if NETSTACK_CONF_WITH_IPV6
  {
    uip_ds6_addr_t *lladdr;
    memcpy(&uip_lladdr.addr, &linkaddr_node_addr, sizeof(uip_lladdr.addr));
    process_start(&tcpip_process, NULL);

    lladdr = uip_ds6_get_link_local(-1);
    LOG_INFO("Tentative link-local IPv6 address: ");
    LOG_INFO_6ADDR(lladdr != NULL ? &lladdr->ipaddr : NULL);
    LOG_INFO_("\n");
  }
#endif /* NETSTACK_CONF_WITH_IPV6 */

  platform_init_stage_three();

#if BUILD_WITH_RPL_BORDER_ROUTER
  rpl_border_router_init();
  LOG_DBG("With RPL Border Router\n");
#endif /* BUILD_WITH_RPL_BORDER_ROUTER */

#if BUILD_WITH_ORCHESTRA
  orchestra_init();
  LOG_DBG("With Orchestra\n");
#endif /* BUILD_WITH_ORCHESTRA */

#if BUILD_WITH_SHELL
  serial_shell_init();
  LOG_DBG("With Shell\n");
#endif /* BUILD_WITH_SHELL */

#if BUILD_WITH_COAP
  coap_engine_init();
  LOG_DBG("With CoAP\n");
#endif /* BUILD_WITH_COAP */

#if BUILD_WITH_SNMP
  snmp_init();
  LOG_DBG("With SNMP\n");
#endif /* BUILD_WITH_SNMP */

#if BUILD_WITH_SIMPLE_ENERGEST
  simple_energest_init();
#endif /* BUILD_WITH_SIMPLE_ENERGEST */

#if BUILD_WITH_TSCH_CS
  /* Initialize the channel selection module */
  tsch_cs_adaptations_init();
#endif /* BUILD_WITH_TSCH_CS */

  autostart_start(autostart_processes);

  watchdog_start();

#if PLATFORM_PROVIDES_MAIN_LOOP
  platform_main_loop();
#else
  while(1) {
    uint8_t r;
    do {
      r = process_run();
      watchdog_periodic();
    } while(r > 0);

    platform_idle();
  }
#endif

  return 0;
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 */
