<?php
/*
 * SAMS (Squid Account Management System)
 * Author: Dmitry Chemerik chemerik@mail.ru
 * (see the file 'main.php' for license details)
 */
  global $SAMSConf;
  global $PROXYConf;
  global $USERConf;
  global $TRANGEConf;
  global $SHABLONConf;
  require('./dbclass.php');
  require('./samsclass.php');
  require('./tools.php');
  //require('./str/grouptray.php');

  $SAMSConf=new SAMSCONFIG();
$SAMSConf->access=2;

 $filename="";
 $sday=0;
 $smon=0;
 $syea=0;
 $shou=0;
 $eday=0;
 $emon=0;
 $eyea=0;
 $ehou=0;
 $sdate=0; 
 $edate=0;
 $user="";

if(isset($_GET["show"]))    $user=$_GET["show"];
if(isset($_GET["filename"])) $filename=$_GET["filename"];
if(isset($_GET["function"])) $function=$_GET["function"];
 if(isset($_GET["id"])) $proxy_id=$_GET["id"];

 $cookie_user="";
 $cookie_passwd="";
 $cookie_domainuser="";
 $cookie_gauditor="";
 if(isset($HTTP_COOKIE_VARS['user'])) $cookie_user=$HTTP_COOKIE_VARS['user'];
 if(isset($HTTP_COOKIE_VARS['passwd'])) $cookie_passwd=$HTTP_COOKIE_VARS['passwd'];
 if(isset($HTTP_COOKIE_VARS['domainuser'])) $cookie_domainuser=$HTTP_COOKIE_VARS['domainuser'];
 if(isset($HTTP_COOKIE_VARS['gauditor'])) $cookie_gauditor=$HTTP_COOKIE_VARS['gauditor'];
	if(isset($HTTP_COOKIE_VARS['userid'])) $SAMSConf->USERID=$HTTP_COOKIE_VARS['userid'];
	if(isset($HTTP_COOKIE_VARS['webaccess'])) $SAMSConf->USERWEBACCESS=$HTTP_COOKIE_VARS['webaccess'];
 if($SAMSConf->PHPVER<5)
   {
	$SAMSConf->adminname=UserAuthenticate($cookie_user,$cookie_passwd);
	$SAMSConf->domainusername=$cookie_domainuser;
	$SAMSConf->groupauditor=$cookie_gauditor;
   }  
 else
   {
	$SAMSConf->adminname=UserAuthenticate($_COOKIE['user'],$_COOKIE['passwd']);
	$SAMSConf->domainusername=$_COOKIE['domainuser'];
	$SAMSConf->groupauditor=$_COOKIE['gauditor'];
	$SAMSConf->USERID=$_COOKIE['userid'];
	$SAMSConf->USERWEBACCESS=$_COOKIE['webaccess'];
   }  
 $SAMSConf->access=UserAccess();

  $lang="./lang/lang.$SAMSConf->LANG";
  require($lang);
  print("<html><head>\n");
  print("<META  content=\"text/html; charset=$CHARSET\" http-equiv='Content-Type'>");
  print("<META HTTP-EQUIV=\"expires\" CONTENT=\"THU, 01 Jan 1970 00:00:01 GMT\">");
  print("<META HTTP-EQUIV=\"pragma\" CONTENT=\"no-cache\">");
  print("<link rel=\"STYLESHEET\" type=\"text/css\" href=\"$SAMSConf->ICONSET/tree.css\">\n");

  print("</head>\n");
  print("<body LINK=\"#ffffff\" VLINK=\"#ffffff\" >\n");

  if(strstr($filename,"proxy"))
	{
	require('./proxyclass.php');
	$PROXYConf=new SAMSPROXY($proxy_id);
	//$PROXYConf->PrintProxyClass();
	}
  if(strstr($filename,"userb")||strstr($filename,"usertray"))
	{
	 if(isset($_GET["id"])) $id=$_GET["id"];
	require('./userclass.php');
	$USERConf=new SAMSUSER($id);
	//$PROXYConf->PrintProxyClass();
	}
  if(strstr($filename,"trange"))
	{
	require('./trangeclass.php');
	$TRANGEConf=new SAMSTRANGE($proxy_id);
	//$PROXYConf->PrintProxyClass();
	}
  if(strstr($filename,"shablon"))
	{
	 if(isset($_GET["id"])) $id=$_GET["id"];
	require('./shablonclass.php');
	$SHABLONConf=new SAMSSHABLON($id);
	//$PROXYConf->PrintProxyClass();
	}
//echo "filename=$filename";
//  if(stristr($filename,".php" )==FALSE)
//    {
//      $filename="";
//      exit(0);
//    }
  if(stristr($filename,".php" )==FALSE) 
    {
      $filename="";
    }
  $req="src/$filename";
  if(strlen($req)>4)
    {
      require($req);
    }
  $function();



print("</body></html>\n");

?>
