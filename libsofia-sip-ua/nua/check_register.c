/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@CFILE check_register.c
 *
 * @brief Check-driven tester for Sofia SIP User Agent library
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2008 Nokia Corporation.
 */

#include "config.h"

#include "check_nua.h"

#include "test_s2.h"

#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/soa.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_tag_io.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static nua_t *nua;

static void register_setup(void)
{
  nua = s2_nua_setup(TAG_END());
}

static void register_teardown(void)
{
  nua_shutdown(nua);
  fail_unless(s2_check_event(nua_r_shutdown, 200));
  s2_nua_teardown();
}

/* ---------------------------------------------------------------------- */

START_TEST(register_1_0)
{
  nua_handle_t *nh = nua_handle(nua, NULL, TAG_END());
  struct message *m;

  s2_case("1.3", "Failed Register", "REGISTER returned 403 response");

  nua_register(nh, TAG_END());

  fail_unless((m = s2_wait_for_request(SIP_METHOD_REGISTER)) != NULL, NULL);

  s2_respond_to(m, NULL,
		SIP_403_FORBIDDEN,
		TAG_END());
  s2_free_message(m);

  nua_handle_destroy(nh);

} END_TEST


START_TEST(register_1_1_1)
{
  s2_case("1.1.1", "Basic Register", "REGISTER returning 200 OK");

  s2_register_setup();

  s2_register_teardown();

} END_TEST


START_TEST(register_1_1_2)
{
  nua_handle_t *nh;
  struct message *m;

  s2_case("1.1.2", "Register with dual authentication",
	  "Register, authenticate");

  nh = nua_handle(nua, NULL, TAG_END());

  nua_register(nh, TAG_END());

  m = s2_wait_for_request(SIP_METHOD_REGISTER); fail_if(!m);
  s2_respond_to(m, NULL,
		SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_free_message(m);
  s2_check_event(nua_r_register, 407);

  nua_authenticate(nh, NUTAG_AUTH(s2_auth_credentials), TAG_END());

  m = s2_wait_for_request(SIP_METHOD_REGISTER); fail_if(!m);
  s2_respond_to(m, NULL,
		SIP_401_UNAUTHORIZED,
		SIPTAG_WWW_AUTHENTICATE_STR(s2_auth2_digest_str),
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_free_message(m);
  s2_check_event(nua_r_register, 401);

  nua_authenticate(nh, NUTAG_AUTH(s2_auth2_credentials), TAG_END());

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m);
  fail_if(!m->sip->sip_authorization);
  fail_if(!m->sip->sip_proxy_authorization);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		TAG_END());
  s2_free_message(m);

  assert(s2->registration->contact != NULL);
  s2_check_event(nua_r_register, 200);

  s2->registration->nh = nh;

  s2_register_teardown();

} END_TEST

/* ---------------------------------------------------------------------- */

static char const *receive_natted = "received=4.255.255.9";

/* Return Via that looks very natted */
static sip_via_t *natted_via(struct message *m)
{
  su_home_t *h;
  sip_via_t *via;

  h = msg_home(m->msg);
  via = sip_via_dup(h, m->sip->sip_via);
  msg_header_replace_param(h, via->v_common, receive_natted);

  if (via->v_protocol == sip_transport_udp)
    msg_header_replace_param(h, via->v_common, "rport=9");

  if (via->v_protocol == sip_transport_tcp && via->v_rport) {
    tp_name_t const *tpn = tport_name(m->tport);
    char *rport = su_sprintf(h, "rport=%s", tpn->tpn_port);
    msg_header_replace_param(h, via->v_common, rport);
  }

  return via;
}

/* ---------------------------------------------------------------------- */

START_TEST(register_1_2_1) {
  nua_handle_t *nh;
  struct message *m;

  s2_case("1.2.1", "Register behind NAT",
	  "Register through NAT, detect NAT, re-REGISTER");

  nh = nua_handle(nua, NULL, TAG_END());

  nua_register(nh, TAG_END());

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m);
  fail_if(!m->sip->sip_contact || m->sip->sip_contact->m_next);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  assert(s2->registration->contact != NULL);
  s2_check_event(nua_r_register, 100);

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m);
  fail_if(!m->sip->sip_contact || !m->sip->sip_contact->m_next);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  fail_unless(s2->registration->contact != NULL);
  fail_if(s2->registration->contact->m_next != NULL);
  s2_check_event(nua_r_register, 200);

  s2->registration->nh = nh;

  s2_register_teardown();

} END_TEST


static nua_handle_t *make_auth_natted_register(
  nua_handle_t *nh,
  tag_type_t tag, tag_value_t value, ...)
{
  struct message *m;

  ta_list ta;
  ta_start(ta, tag, value);
  nua_register(nh, ta_tags(ta));
  ta_end(ta);

  m = s2_wait_for_request(SIP_METHOD_REGISTER); fail_if(!m);
  s2_respond_to(m, NULL,
		SIP_401_UNAUTHORIZED,
		SIPTAG_WWW_AUTHENTICATE_STR(s2_auth_digest_str),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  s2_check_event(nua_r_register, 401);

  nua_authenticate(nh, NUTAG_AUTH(s2_auth_credentials), TAG_END());

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m);
  fail_if(!m->sip->sip_authorization);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  assert(s2->registration->contact != NULL);
  s2_check_event(nua_r_register, 200);

  return nh;
}

START_TEST(register_1_2_2_1)
{
  nua_handle_t *nh = nua_handle(nua, NULL, TAG_END());

  s2_case("1.2.2.1", "Register behind NAT",
	  "Authenticate, outbound activated");

  mark_point();
  make_auth_natted_register(nh, TAG_END());
  s2->registration->nh = nh;
  s2_register_teardown();
}
END_TEST

START_TEST(register_1_2_2_2)
{
  nua_handle_t *nh = nua_handle(nua, NULL, TAG_END());
  struct message *m;

  s2_case("1.2.2.2", "Register behind NAT",
	  "Authenticate, outbound activated, "
	  "authenticate OPTIONS probe, "
	  "NAT binding change");

  mark_point();
  make_auth_natted_register(nh, TAG_END());
  s2->registration->nh = nh;

  mark_point();

  m = s2_wait_for_request(SIP_METHOD_OPTIONS);
  fail_if(!m);
  s2_respond_to(m, NULL,
		SIP_407_PROXY_AUTH_REQUIRED,
		SIPTAG_VIA(natted_via(m)),
		SIPTAG_PROXY_AUTHENTICATE_STR(s2_auth_digest_str),
		TAG_END());
  s2_free_message(m);
  mark_point();

  m = s2_wait_for_request(SIP_METHOD_OPTIONS);
  fail_if(!m); fail_if(!m->sip->sip_proxy_authorization);
  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  su_root_step(s2->root, 20); su_root_step(s2->root, 20);
  s2_fast_forward(120);	  /* Default keepalive interval */
  mark_point();

  m = s2_wait_for_request(SIP_METHOD_OPTIONS);
  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  su_root_step(s2->root, 20); su_root_step(s2->root, 20);
  s2_fast_forward(120);	  /* Default keepalive interval */
  mark_point();

  receive_natted = "received=4.255.255.10";

  m = s2_wait_for_request(SIP_METHOD_OPTIONS);
  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  s2_check_event(nua_i_outbound, 0);

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m); fail_if(!m->sip->sip_authorization);
  fail_if(!m->sip->sip_contact || !m->sip->sip_contact->m_next);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  s2_check_event(nua_r_register, 200);

  fail_unless(s2->registration->contact != NULL);
  fail_if(s2->registration->contact->m_next != NULL);

  s2_register_teardown();

} END_TEST


START_TEST(register_1_2_2_3)
{
  nua_handle_t *nh = nua_handle(nua, NULL, TAG_END());
  struct message *m;

  s2_case("1.2.2.3", "Register behind NAT",
	  "Authenticate, outbound activated, "
	  "detect NAT binding change when re-REGISTERing");

  mark_point();
  make_auth_natted_register(nh,
			    NUTAG_OUTBOUND("no-options-keepalive"),
			    TAG_END());
  s2->registration->nh = nh;

  receive_natted = "received=4.255.255.10";

  s2_fast_forward(3600);
  mark_point();

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m); fail_if(!m->sip->sip_authorization);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  s2_check_event(nua_r_register, 100);

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m); fail_if(!m->sip->sip_authorization);
  fail_if(!m->sip->sip_contact || !m->sip->sip_contact->m_next);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  fail_unless(s2->registration->contact != NULL);
  fail_if(s2->registration->contact->m_next != NULL);

  s2_check_event(nua_r_register, 200);

  s2_register_teardown();

} END_TEST


START_TEST(register_1_2_3) {
  nua_handle_t *nh;
  struct message *m;

  s2_case("1.2.3", "Register behind NAT",
	  "Outbound activated by error response");

  nh = nua_handle(nua, NULL, TAG_END());
  nua_register(nh, TAG_END());

  mark_point();

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m);
  fail_if(!m->sip->sip_contact || m->sip->sip_contact->m_next);

  s2_respond_to(m, NULL,
		400, "Bad Contact",
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  s2_check_event(nua_r_register, 100);

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  fail_unless(s2->registration->contact != NULL);
  fail_if(s2->registration->contact->m_next != NULL);
  s2_check_event(nua_r_register, 200);

  s2->registration->nh = nh;

  s2_register_teardown();

} END_TEST


/* ---------------------------------------------------------------------- */

START_TEST(register_1_3_1)
{
  nua_handle_t *nh;
  struct message *m;

  s2_case("1.3.1", "Register over TCP via NAT",
	  "REGISTER via TCP, detect NTA, re-REGISTER");

  nh = nua_handle(nua, NULL, TAG_END());

  nua_register(nh, NUTAG_PROXY(s2->tcp.contact->m_url), TAG_END());

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m); fail_if(!m->sip->sip_contact || m->sip->sip_contact->m_next);
  fail_if(!tport_is_tcp(m->tport));
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  assert(s2->registration->contact != NULL);
  s2_check_event(nua_r_register, 100);

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m);
  fail_if(!m->sip->sip_contact || !m->sip->sip_contact->m_next);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  fail_unless(s2->registration->contact != NULL);
  fail_if(s2->registration->contact->m_next != NULL);
  fail_unless(
    url_has_param(s2->registration->contact->m_url, "transport=tcp"));
  s2_check_event(nua_r_register, 200);

  s2->registration->nh = nh;

  s2_register_teardown();

} END_TEST


START_TEST(register_1_3_2_1)
{
  nua_handle_t *nh = nua_handle(nua, NULL, TAG_END());

  s2_case("1.3.2.1", "Register behind NAT",
	  "Authenticate, outbound activated");

  mark_point();
  s2->registration->nh = nh;
  make_auth_natted_register(nh, NUTAG_PROXY(s2->tcp.contact->m_url), TAG_END());
  fail_if(!tport_is_tcp(s2->registration->tport));
  s2_register_teardown();
}
END_TEST


START_TEST(register_1_3_2_2)
{
  nua_handle_t *nh = nua_handle(nua, NULL, TAG_END());
  struct message *m;

  s2_case("1.3.2.2", "Register behind NAT with TCP",
	  "Detect NAT over TCP using rport. "
	  "Authenticate, detect NAT, "
	  "close TCP at server, wait for re-REGISTERs.");

  nua_set_params(nua, NTATAG_TCP_RPORT(1), TAG_END());
  s2_check_event(nua_r_set_params, 200);

  mark_point();
  s2->registration->nh = nh;
  make_auth_natted_register(
    nh, NUTAG_PROXY(s2->tcp.contact->m_url),
    NUTAG_OUTBOUND("no-options-keepalive, no-validate"),
    TAG_END());
  fail_if(!tport_is_tcp(s2->registration->tport));
  tport_shutdown(s2->registration->tport, 2);

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m); fail_if(!m->sip->sip_authorization);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  /* The "NAT binding" changed when new TCP connection is established */
  /* => NUA re-REGISTERs with newly detected contact */
  s2_check_event(nua_r_register, 100);

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  fail_if(!m); fail_if(!m->sip->sip_authorization);
  fail_if(!m->sip->sip_contact || !m->sip->sip_contact->m_next);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		SIPTAG_VIA(natted_via(m)),
		TAG_END());
  s2_free_message(m);

  s2_check_event(nua_r_register, 200);

  fail_unless(s2->registration->contact != NULL);
  fail_if(s2->registration->contact->m_next != NULL);

  s2_register_teardown();
}
END_TEST

/* ---------------------------------------------------------------------- */

TCase *register_tcase(void)
{
  TCase *tc = tcase_create("1 - REGISTER");
  /* Each testcase is run in different process */
  tcase_add_checked_fixture(tc, register_setup, register_teardown);
  {
    tcase_add_test(tc, register_1_0);
    tcase_add_test(tc, register_1_1_1);
    tcase_add_test(tc, register_1_1_2);
    tcase_add_test(tc, register_1_2_1);
    tcase_add_test(tc, register_1_2_2_1);
    tcase_add_test(tc, register_1_2_2_2);
    tcase_add_test(tc, register_1_2_2_3);
    tcase_add_test(tc, register_1_2_3);
    tcase_add_test(tc, register_1_3_1);
    tcase_add_test(tc, register_1_3_2_1);
    tcase_add_test(tc, register_1_3_2_2);
  }
  tcase_set_timeout(tc, 5);
  return tc;
}

void check_register_cases(Suite *suite)
{
  suite_add_tcase(suite, register_tcase());
}
