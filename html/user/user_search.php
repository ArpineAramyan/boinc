<?php
// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2008 University of California
//
// BOINC is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// BOINC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

require_once("../inc/boinc_db.inc");
require_once("../inc/util.inc");
require_once("../inc/user.inc");

function show_user($user) {
    echo "
        <tr>
        <td>", user_links($user, BADGE_HEIGHT_MEDIUM), " (ID $user->id)</td>
    ";
    if ($user->teamid) {
        $team = BoincTeam::lookup_id($user->teamid);
        echo "
            <td> <a href=team_display.php?teamid=$team->id>$team->name</a> </td>
        ";
    } else {
        echo "<td><br></td>";
    }
    echo "
        <td align=right>", format_credit($user->expavg_credit), "</td>
        <td align=right>", format_credit_large($user->total_credit), "</td>
        <td>", $user->country, "</td>
        <td>", time_str($user->create_time),"</td>
        </tr>
    ";
}

function user_search_form() {
    page_head(tra("User search"));
    echo "<form name=f method=get action=user_search.php>
        <input type=hidden name=action value=search>
    ";
    start_table();
    row1(tra("Filters"), 2, "heading");
    row2(
        tra("User name starts with"),
        '<input class="form-control" type="text" name="search_string">'
    );
    row2_init(tra("Country"));
    echo "<select class=\"form-control\" name=\"country\"><option value=\"any\" selected>".tra("Any")."</option>";
    echo country_select_options("asdf");
    echo "</select></td></tr>";
    row2(tra("With profile?"),
        "<input type=radio name=profile value=either checked=1> ".tra("Either")."
        &nbsp;<input type=radio name=profile value=no> ".tra("No")."
        &nbsp;<input type=radio name=profile value=yes> ".tra("Yes")."
    ");
    row2(tra("On a team?"),
        "<input type=radio name=team value=either checked=1> ".tra("Either")."
        &nbsp;<input type=radio name=team value=no> ".tra("No")."
        &nbsp;<input type=radio name=team value=yes> ".tra("Yes")."
    ");
    row1(tra("Ordering"), 2, "heading");
    row2(tra("Decreasing sign-up time"), "<input type=radio name=search_type value=\"date\" checked>");
    row2(tra("Decreasing average credit"), "<input type=radio name=search_type value=\"rac\">");
    row2(tra("Decreasing total credit"), "<input type=radio name=search_type value=\"total\">");
    row2("", "<input class=\"btn btn-success\" type=submit name=action value=".tra("Search").">");
    end_table();
    echo "
        <script>document.f.search_string.focus()</script>
    ";

    page_tail();
}

function search_action() {
    $where = "true";
    $search_string = get_str('search_string');
    if (strlen($search_string)) {
        if (strlen($search_string)<3) {
            error_page(tra("search string must be at least 3 characters"));
        }
        $s = BoincDb::escape_string($search_string);
        $s = escape_pattern($s);
        $where .= " and name like '$s%'";
    }
    $country = get_str('country');
    if ($country != 'any') {
        $s = BoincDb::escape_string($country);
        $where .= " and country='$s'";
    }
    $t = get_str('team');
    if ($t == 'yes') {
        $where .= " and teamid<>0";
    } else if ($t == 'no') {
        $where .= " and teamid=0";
    }
    $t = get_str('profile');
    if ($t == 'yes') {
        $where .= " and has_profile<>0";
    } else if ($t == 'no') {
        $where .= " and has_profile=0";
    }

    $search_type = get_str('search_type', true);
    $order_clause = "id desc";
    if ($search_type == 'rac') {
        $order_clause ="expavg_credit desc";
    } else if ($search_type == 'total') {
        $order_clause ="total_credit desc";
    }

    $fields = "id, create_time, name, country, total_credit, expavg_credit, teamid, url, has_profile, donated";
    $users = BoincUser::enum_fields($fields, $where, "order by $order_clause limit 100");
    page_head(tra("User search results"));
    $n=0;
    foreach ($users as $user) {
        if ($n==0) {
            start_table('table-striped');
            row_heading_array(
                array(
                    tra("Name"),
                    tra("Team"),
                    tra("Average credit"),
                    tra("Total credit"),
                    tra("Country"),
                    tra("Joined")
                ),
                array(null, null, ALIGN_RIGHT, ALIGN_RIGHT, null, null)
            );
        }
        show_user($user);
        $n++;
    }
    end_table();
    if (!$n) {
        echo tra("No users match your search criteria.");
    }
    page_tail();
}

$action = get_str('action', true);
if ($action) {
    search_action();
} else {
    user_search_form();
}

$cvs_version_tracker[]="\$Id: user_search.php 13586 2007-09-13 09:46:36Z Rytis $";  //Generated automatically - do not edit
?>
