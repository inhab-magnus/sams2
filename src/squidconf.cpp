/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include <sstream>

#include "squidconf.h"

#include "debug.h"
#include "samsconfig.h"
#include "samsuserlist.h"
#include "samsuser.h"
#include "urlgrouplist.h"
#include "urlgroup.h"
#include "url.h"
#include "proxy.h"
#include "templatelist.h"
#include "template.h"
#include "timerangelist.h"
#include "timerange.h"
#include "delaypoollist.h"
#include "delaypool.h"
#include "tools.h"

SquidConf::SquidConf()
{
  DEBUG (DEBUG7, "[" << this << "->" << __FUNCTION__ << "]");
}


SquidConf::~SquidConf()
{
  DEBUG (DEBUG7, "[" << this << "->" << __FUNCTION__ << "]");
}

bool SquidConf::defineAccessRules()
{
  return defineACL ();
}

bool SquidConf::defineACL ()
{
  int err;

  string squidconfdir = SamsConfig::getString (defSQUIDCONFDIR, err);

  if (squidconfdir.empty ())
    {
      ERROR (defSQUIDCONFDIR << " not defined. Check config file.");
      return false;
    }

  string squidconffile = squidconfdir + "/squid.conf";
  string squidbakfile = squidconfdir + "/squid.conf.bak";

  if (!fileCopy (squidconffile, squidbakfile))
    return false;

  ofstream fout;
  fout.open (squidconffile.c_str (), ios::out);
  if (!fout.is_open ())
    {
      ERROR ("Unable to create file " << squidconffile);
      return false;
    }

  ifstream fin;
  fin.open (squidbakfile.c_str (), ios::in);
  if (!fin.is_open ())
    {
      ERROR ("Unable to open file " << squidbakfile);
      fout.close();
      return false;
    }

  string line;
  string nextline;
  string current_tag = "unknown";
  vector <string> v;

  vector <Template *> tpls = TemplateList::getList ();
  vector <Template *>::iterator tpls_it;
  vector <TimeRange*> times = TimeRangeList::getList ();
  vector <TimeRange*>::const_iterator time_it;
  vector <long> time_ids;
  vector <long> group_ids;
  Template * tpl;
  TimeRange * trange;
  uint j;
  bool haveBlockedUsers = false;
  Proxy::RedirType redir_type = Proxy::getRedirectType ();

  ofstream fncsa;
  string ncsafile = squidconfdir + "/sams2.ncsa";
  Proxy::usrAuthType authType;

  while (fin.good ())
    {
      getline (fin, line);
      if (line.empty ())
        {
          if (!fin.eof ())
            fout << endl;
          continue;
        }

      // Строка от нашей старой конфигурации - игнорируем ее
      if (line.find ("Sams2") != string::npos)
        continue;

      //Ограничение скорости настраивается демоном
      if (line.find ("delay_") == 0 && Proxy::useDelayPools())
        continue;

      if (line[0] == '#' && line.find ("TAG:") != string::npos)
        {
          fout << line << endl;
          nextline = skipComments (fin, fout);

          Split(line, " \t\n", v);
          current_tag = "unknown";
          if (v[2] == "acl")
            {
              current_tag = "acl";
              DEBUG (DEBUG2, "Found TAG: acl");
            }
          else if (v[2] == "http_access")
            {
              current_tag = "http_access";
              DEBUG (DEBUG2, "Found TAG: http_access");
            }
          else if ((v[2] == "url_rewrite_access") || (v[2] == "redirector_access"))
            {
              current_tag = v[2];
              DEBUG (DEBUG2, "Found TAG: " << current_tag);
            }
          else if (v[2] == "delay_pools")
            {
              current_tag = v[2];
              DEBUG (DEBUG2, "Found TAG: " << current_tag);
            }
          else if (v[2] == "delay_class")
            {
              current_tag = v[2];
              DEBUG (DEBUG2, "Found TAG: " << current_tag);
            }
          else if (v[2] == "delay_access")
            {
              current_tag = v[2];
              DEBUG (DEBUG2, "Found TAG: " << current_tag);
            }
          else if (v[2] == "delay_parameters")
            {
              current_tag = v[2];
              DEBUG (DEBUG2, "Found TAG: " << current_tag);
            }

          if (current_tag == "acl")
            {
              // Создаем списки временных границ
              for (time_it = times.begin (); time_it != times.end (); time_it++)
                {
                      if ((*time_it)->isFullDay ())
                        continue;
                      fout << "acl Sams2Time" << (*time_it)->getId () << " time " << (*time_it)->getDays () << " ";
                      if ((*time_it)->hasMidnight ())
                        fout << (*time_it)->getEndTimeStr () << "-" << (*time_it)->getStartTimeStr () << endl;
                      else
                        fout << (*time_it)->getStartTimeStr () << "-" << (*time_it)->getEndTimeStr () << endl;
                }

              // Создаем списки пользователей
              vector<SAMSUser *> users;
              for (tpls_it = tpls.begin (); tpls_it != tpls.end (); tpls_it++)
                {
                  tpl = *tpls_it;
                  SAMSUserList::getUsersByTemplate (tpl->getId (), users);
                  if (users.empty ())
                    continue;

                  DEBUG(DEBUG2, "Processing "<<users.size()<<" user[s] in template " << tpl->getId ());

                  string method;
                  authType = tpl->getAuth ();
                  if (authType == Proxy::AUTH_IP)
                    method = "src";
                  else
                    method = "proxy_auth";

                  if ((authType == Proxy::AUTH_NCSA) && (!fncsa.is_open ()))
                    {
                      fncsa.open (ncsafile.c_str (), ios::out | ios::trunc);
                      if (!fncsa.is_open ())
                        {
                          ERROR ("Unable to open file " << ncsafile);
                        }
                    }

                  /// TODO Пользователи могут быть заблокированы из разных шаблонов с разными типами авторизации
                  vector<SAMSUser *>::iterator it;
                  for (it = users.begin(); it != users.end(); it++)
                    {
                      if ( (redir_type == Proxy::REDIR_NONE) &&
                           ((*it)->getEnabled() != SAMSUser::STAT_ACTIVE) &&
                           ((*it)->getEnabled() != SAMSUser::STAT_LIMITED))
                        {
                          // Блокируем пользователей в squid.conf только если никакой редиректор не используется.
                          haveBlockedUsers = true;
                          fout << "acl Sams2BlockedUsers " << method << " " << *(*it) << endl;
                        }
                      else
                        {
                          if ((authType == Proxy::AUTH_NCSA) && (fncsa.is_open ()))
                            fncsa << *(*it) << ":" << (*it)->getPassword () << endl;
                          fout << "acl Sams2Template" << tpl->getId () << " " << method << " " << *(*it) << endl;
                        }
                    }

                  if (fncsa.is_open ())
                    {
                      fncsa.close ();
/* Смена владельца не актуальна, т.к. демоны авторизации работают под root
                      string chown_cmd = "chown squid " + ncsafile;
                      if (system (chown_cmd.c_str ()))
                        {
                          WARNING ("Unable to change owner of " << ncsafile);
                        }
*/
                    }

//                  if (redir_type == Proxy::REDIR_INTERNAL)
//                    continue;

                }

              if (redir_type != Proxy::REDIR_INTERNAL)
                {
                  vector<UrlGroup *>::iterator grp_it;
                  vector<UrlGroup *> grps = UrlGroupList::getAllGroups ();
                  for (grp_it = grps.begin (); grp_it != grps.end (); grp_it++)
                    {
                      long grp_id = (*grp_it)->getId ();
                      switch ( (*grp_it)->getAccessType () )
                        {
                          case UrlGroup::ACC_DENY:
                            fout << "acl Sams2Deny" << grp_id << " dstdom_regex " << *(*grp_it) << endl;
                            break;
                          case UrlGroup::ACC_ALLOW:
                            fout << "acl Sams2Allow" << grp_id << " dstdom_regex " << *(*grp_it) << endl;
                            break;
                          case UrlGroup::ACC_REGEXP:
                            fout << "acl Sams2Regexp" << grp_id << " url_regex " << *(*grp_it) << endl;
                            break;
                          case UrlGroup::ACC_REDIR:
                            break;
                          case UrlGroup::ACC_REPLACE:
                            break;
                          case UrlGroup::ACC_FILEEXT:
                            fout << "acl Sams2Fileext" << grp_id << " urlpath_regex " << *(*grp_it) << endl;
                            break;
                        }
                    }
                }

            } // if (current_tag == "acl")

          if (current_tag == "http_access")
            {
              fout << "# Setup Sams2 HTTP Access here" << endl;
              vector <long> times;
              basic_stringstream < char >restriction;

              if (haveBlockedUsers)
                fout << "http_access deny Sams2BlockedUsers" << endl;

              vector<SAMSUser *> users;
              for (tpls_it = tpls.begin (); tpls_it != tpls.end (); tpls_it++)
                {
                  tpl = *tpls_it;
                  SAMSUserList::getUsersByTemplate (tpl->getId (), users);
                  if (users.empty ())
                    continue;

                  DEBUG(DEBUG_DAEMON, "Processing template " << tpl->getId ());

                  restriction.str("");

                  if (redir_type != Proxy::REDIR_INTERNAL)
                    {
                      time_ids = tpl->getTimeRangeIds ();
                      for (j = 0; j < time_ids.size(); j++)
                        {
                          trange = TimeRangeList::getTimeRange(time_ids[j]);
                          if (!trange)
                            continue;
                          if (trange->isFullDay ())
                            continue;
                          if (trange->hasMidnight ())
                            restriction << " !Sams2Time" << time_ids[j];
                          else
                            restriction << " Sams2Time" << time_ids[j];
                        }

                      //Определяем разрешающие и запрещающие правила для текущего шаблона
                      group_ids = tpl->getUrlGroupIds ();
                      for (j = 0; j < group_ids.size(); j++)
                        {
                          UrlGroup * grp = UrlGroupList::getUrlGroup(group_ids[j]);
                          if (!grp)
                            continue;
                          switch (grp->getAccessType ())
                            {
                              case UrlGroup::ACC_DENY:
                                restriction << " !Sams2Deny" << group_ids[j];
                                break;
                              case UrlGroup::ACC_ALLOW:
                                restriction << " Sams2Allow" << group_ids[j];
                                break;
                              case UrlGroup::ACC_REGEXP:
                                restriction << " !Sams2Regexp" << group_ids[j];
                                break;
                              case UrlGroup::ACC_REDIR:
                                break;
                              case UrlGroup::ACC_REPLACE:
                                break;
                              case UrlGroup::ACC_FILEEXT:
                                restriction << " !Sams2Fileext" << group_ids[j];
                                break;
                            }
                        }
                    }

                  if (redir_type == Proxy::REDIR_INTERNAL)
                    fout << "http_access allow Sams2Template" << tpl->getId () << endl;
                  else if (SAMSUserList::activeUsersInTemplate (tpl->getId ()) > 0)
                    fout << "http_access allow Sams2Template" << tpl->getId () << restriction.str() << endl;
                }
            } //if (current_tag == "http_access")

          if (current_tag == "url_rewrite_access" || current_tag == "redirector_access")
            {
              Url proxy_url;
              proxy_url.setUrl (Proxy::getDenyAddr ());
              string proxy_addr = proxy_url.getAddress ();
              if (!proxy_addr.empty ())
                {
                  fout << "acl Sams2Proxy dst " << proxy_addr << endl;
                  fout << current_tag << " deny Sams2Proxy" << endl;
                }
              else
                {
                  WARNING ("Unable to identify proxy address");
                }
            }

          if (current_tag == "delay_pools" && Proxy::useDelayPools())
            {
              fout << "delay_pools " << DelayPoolList::count () << endl;
            }

          if (current_tag == "delay_class" && Proxy::useDelayPools())
            {
              vector<DelayPool*> pools = DelayPoolList::getList ();
              for (unsigned int i=0; i < pools.size (); i++)
                {
                  fout << "delay_class " << i+1 << " " << pools[i]->getClass () << endl;
                }
            }

          if (current_tag == "delay_access" && Proxy::useDelayPools())
            {
              vector<DelayPool*> pools = DelayPoolList::getList ();
              map <long, bool> link;
              map <long, bool>::const_iterator it;
              for (unsigned int i=0; i < pools.size (); i++)
                {
                  link = pools[i]->getTemplates ();
                  for (it = link.begin (); it != link.end (); it++)
                    {
                      if (SAMSUserList::activeUsersInTemplate (it->first) > 0)
                        fout << "delay_access " << i+1 << " " << ((it->second==true)?"deny":"allow") << " Sams2Template" << it->first << endl;
                    }

                  link = pools[i]->getTimeRanges ();
                  for (it = link.begin (); it != link.end (); it++)
                    {
                      trange = TimeRangeList::getTimeRange(it->first);
                      if (!trange)
                        continue;
                      if (trange->isFullDay ())
                        continue;

                      fout << "delay_access " << i+1 << " " << ((it->second==true)?"deny":"allow");
                      if (trange->hasMidnight ())
                        fout << " !Sams2Time" << it->first;
                      else
                        fout << " Sams2Time" << it->first;
                      fout << endl;
                    }

                  fout << "delay_access " << i+1 << " deny all" << endl;
                }
            }

          if (current_tag == "delay_parameters" && Proxy::useDelayPools())
            {
              vector<DelayPool*> pools = DelayPoolList::getList ();
              long agg1, agg2;
              long net1, net2;
              long ind1, ind2;
              for (unsigned int i=0; i < pools.size (); i++)
                {
                  pools[i]->getAggregateParams (agg1, agg2);
                  pools[i]->getNetworkParams (net1, net2);
                  pools[i]->getIndividualParams (ind1, ind2);

                  fout << "delay_parameters " << i+1 << " ";
                  switch (pools[i]->getClass ())
                    {
                      case 1:
                        fout << agg1 << "/" << agg2 << endl;
                        break;
                      case 2:
                        fout << agg1 << "/" << agg2 << " " << ind1 << "/" << ind2 << endl;
                        break;
                      case 3:
                        fout << agg1 << "/" << agg2 << " " << net1 << "/" << net2 << " " << ind1 << "/" << ind2 << endl;
                        break;
                      default:
                        break;
                    }
                }
            }

          if (nextline.find ("delay_") == 0 && Proxy::useDelayPools())
            continue;

          fout << nextline << endl;
        }
      else
        {
          fout << line << endl;
        }
    }

  fin.close ();
  fout.close ();

  return true;
}

string SquidConf::skipComments (ifstream & in, ofstream & out)
{
  string line;
  getline (in, line);
  while (in.good () && line[0] == '#')
    {
      if (line.find ("Sams2") == string::npos)
        out << line << endl;
      getline (in, line);
      if (line.find ("Sams2") != string::npos)
        line[0] = '#';
    }
  return line;
}

