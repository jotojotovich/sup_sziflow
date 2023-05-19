<?php

/**
 * @file
 * Вспомогательные функции модуля sup_sziflow.
 */


/**
 * Извлекает из отчета программы инвентаризации данные о СЗИ, а также ряд других метаданных, 
 * необходимых для составления актов.
 *
 * @todo Вероятнее всего это необходимо переместить в _inventagent
 *
 * @param $report
 *   Загруженный объект-отчет (sia_report).
 * @return
 *   Ассоциативный массив с распаршенными данными.
 *   - szi - данные по СЗИ - линейный массив из ассоциативных массивов:
 *     - type* - код типа СЗИ;
 *     - version* - версия;
 *     - install_date - штамп времени установки ПО;
 *     - license - лицензионный номер;
 *     - _vn__apid - номер АП сети ViPNet;
 **   - meta - метаданные отчета:
 **     - host_user - пользователь компьютера;
 **     - host_inv - инвентарный номер;
 **     - comment - комментарий;
 */
 
 /* 17052023 */
 
function sup_sziflow_parse_sia_report($report, $options = array()) {
  $options += array(
    'kes__never_hide' => FALSE,
  );

  $result = array('szi' => array());

  // ZIP-архив с отчетом.
  $zip_file = file_load($report->field_sia_report_file[LANGUAGE_NONE][0]['fid']);
  $zip_realpath = drupal_realpath($zip_file->uri);  
  
  // @todo временно.
  if ($xml_contents = srr_inventagent_zipped_file_get_contents($zip_realpath, 'meta.xml')) {
    $xml = new SimpleXMLElement($xml_contents);
    $data = $xml->xpath('/report');
    $data = current($data);
    $result['meta']['host_addr'] = trim($data->HostAddr);
    $result['meta']['host_sticker'] = trim($data->HostSticker);
    $result['meta']['comment'] = trim($data->Comment);
    $result['meta']['host_name'] = trim($data->ComputerName);
    $result['meta']['os_arch'] = trim($data->OSArch);
    $result['meta']['os_build'] = trim($data->OSBuild);
    $result['meta']['os_lang'] = trim($data->OSLang);
    $result['meta']['os_servicepack'] = trim($data->OSServicePack);
    $result['meta']['os_type'] = trim($data->OSType);
    $result['meta']['os_version'] = trim($data->OSVersion);
    $result['meta']['is_server_os'] = in_array($result['meta']['os_version'], array('WIN_2016', 'WIN_2012R2', 'WIN_2012', 'WIN_2008R2', 'WIN_2008', 'WIN_2003'));
  }

  // Пакет: InventorySZI. 
  if ($xml_contents = srr_inventagent_zipped_file_get_contents($zip_realpath, 'InventorySZI (builtin)/data.xml')) {
    $xml = new SimpleXMLElement($xml_contents);
    $data = $xml->xpath('/packagedata');
    $data = current($data);

    // Отдельная информация по Secret Net.
    if (isset($data->SecretNet->ProductNumber) && !empty(trim($data->SecretNet->ProductNumber))) {
      $result['szi']['sn']['license'] = trim($data->SecretNet->ProductNumber);
    }
 
    // Secret Net Studio.
    if (isset($data->SecretNet->ProductLicense) && !empty(trim($data->SecretNet->ProductLicense))) {
      $result['szi']['sns']['license'] = 'нераспознанная лицензия';
      // Для Studio лицензия хранится в ключе ProductLicense. Он представляет собой
      // многострочную запись. Каждая строчка - лицензия на отдельный компонент. Для каждого
      // компонента лицензия своя и содержится она в поле LicID. Как минимум фиксировалось 2
      // формата написания значения этого поля (первый - в более новом по времени файле одной
      // лицензии, второй - в более старом по времени файле _другой_ лицензии):
      // 1. 15248E_014 (№ бумажной лицензии перед подчеркиванием - в одном отчете м.б. несколько
      // разных лицензий, например - основная и для компонентов с демо);
      // 2. 30970b97-f649-45ca-bb50-7605bfdb825e (непонятный UUID).
      // В случае подпадания под первый формат: в акт идет первая попавшаяся за исключением списка 
      // демо-лицензий. Так обсудили 09.07.19 с Холманским В. У Чокли получилось, что поставилась 
      // нормальная лицензия на основные компоненты и демо на МЭ и аутент. соединений. И первой 
      // была на МЭ (демо) и она и пошла в акт. 
      // В другом случае - производится попытка сравнить лицензию с имеющейся цитовской. Если
      // наша - подставляем наш номер, если нет - оставляем нераспознанной.
      $lic_raw = isset($data->SecretNet->ProductLicense) ? trim($data->SecretNet->ProductLicense) : '';
      $lines = explode("\n", $lic_raw);
      $lics_uuids = array();
      $demo_lic = array(
        '145DAC' // 30 мест на МЭ и аутентификацию сетевых соединений.
      );
      foreach ($lines as $line) {
        // Lic.
        if (preg_match('/LicID=([a-z0-9]{7})_\d+/ius', $line, $m) || preg_match('/LicID=([a-z0-9]{6})_\d+/ius', $line, $m)) {
          $cur_lic = drupal_strtoupper($m[1]);
          if (!in_array($cur_lic, $demo_lic)) {
            $result['szi']['sns']['license'] = drupal_strtoupper($m[1]);  
            break;
          }
        }
        // UUID.
        elseif (preg_match('/LicID=([abcdef0-9]{8}\-[abcdef0-9]{4}\-[abcdef0-9]{4}\-[abcdef0-9]{4}\-[abcdef0-9]{12})/ius', $line, $m)) {
          $lics_uuids[] = drupal_strtolower($m[1]);
        }
      }
      if (!empty($lics_uuids)) {
        // Если представлен набор UUID, тогда пытаемся найди один из ЦИТ-овских. Предполагаю,
        // что UUID уникальные. Сравнивать полный набор возможно не очень правильно, т.к. есть
        // вероятность, что были установлены не все компоненты (электронная подпись лицензии
        // идет построчно).
        $cit_lics_uuids = array(
          '0b24c0a4-bcc2-43b9-8278-b7daae05f606',
          '30970b97-f649-45ca-bb50-7605bfdb825e',
          '39db96b9-f5cc-4a8d-8992-1d16c95d96bc',
          '587b9607-964e-406c-be29-38c84200384b',
          '898183d9-1034-4103-9108-c820cc787296',
          'afcfd16b-ebe8-405e-b66c-4e23db29d205',
          'b6e08b35-85b5-4da0-8531-921dab107739',
        );
        $uuid_int = array_intersect($lics_uuids, $cit_lics_uuids);
        if (!empty($uuid_int)) {
          $result['szi']['sns']['license'] = '11B839';
        }
      }
    }

    // Cryptopro JCP.
    if (isset($data->CryptoProJCP->License) && !empty(trim($data->CryptoProJCP->License))) {
      $sn = $data->CryptoProJCP->License;
      $sn = trim(str_replace('/', '', $sn));
      if (strpos($sn, ' ')) {
        $sn = trim(drupal_substr($sn, strpos($sn, ' ')));
      }
      $sn = base64_decode($sn);
      $sn = drupal_strtoupper(str_replace('-', '', $sn));
      if ($sn && (drupal_strlen($sn) >= 25)) {
        $result['szi']['cpjcp']['license'] = drupal_substr($sn, -25);
        $result['szi']['cpjcp']['version'] = !empty($data->CryptoProJCP->Version) 
          ? trim($data->CryptoProJCP->Version)
          : '1.0';
      }
    }    

    // Отдельная информация по ViPNet. Иногда бывает так, что версия тут не отображается (о129279). 
    // Необходимо ее извлекать из списка ПО.
    if (isset($data->ViPNet->Version) && !empty($data->ViPNet->Version)) {
      $data->ViPNet->Version = trim($data->ViPNet->Version);
      if (!empty($data->ViPNet->Version)) {
        $result['szi']['vn']['version'] = trim($data->ViPNet->Version);
      }
    }
    if (isset($data->ViPNet->APId) && !empty($data->ViPNet->APId)) {
      $result['szi']['vn']['_vn__apid'] = trim($data->ViPNet->APId);
    }

    // Отдельная информация по КриптоПРО CSP.
    if (isset($data->CryptoProCSP->Data->item->ProductID) && !empty($data->CryptoProCSP->Data->item->ProductID)) {
      $result['szi']['cpcsp']['license'] = trim($data->CryptoProCSP->Data->item->ProductID);
    }
    if (isset($data->CryptoProCSP->Data->item->DisplayVersion) && !empty($data->CryptoProCSP->Data->item->DisplayVersion)) {
      $result['szi']['cpcsp']['version'] = trim($data->CryptoProCSP->Data->item->DisplayVersion);
    }

    // Отдельная информация по КриптоАРМ.
    // КриптоАРМ версии 5 при вводе нормальной лицензии также хранит и демо ключ (в другой ветке 
    // реестра). Детально не изучалось, но есть вероятность, что на платформах x86 он таки 
    // перезаписывается.
    $data = $xml->xpath('/packagedata/CryptoARM/Data/item');
    if ($data && is_array($data) && !empty($data)) {
      $crarm_lic = array();
      foreach ($data as $lic_info) {
        if (isset($lic_info->LicenseSerialNumber)) {
          $sn = trim($lic_info->LicenseSerialNumber);
          $sn = drupal_strtolower($sn);
          $sn = str_replace('-', '', $sn);
          if (!empty($sn)) {
            $crarm_lic[$sn] = $sn;
          }
        }
      }
      // Если лицензия одна - то она и указывается. Если 2, тогда убирается демка.
      if (!empty($crarm_lic)) {
        $demo = array('td5qcqhwdkgdrqwfckhcrgmqhtfmxmcdcvk');
        if (count($crarm_lic) > 1) {
          foreach ($demo as $demo_sn) {
            unset($crarm_lic[$demo_sn]);
          }
        }
        if (count($crarm_lic) == 1) {
          $result['szi']['crarm']['license'] = current($crarm_lic);
        }
      }
    }
  }

  // Пакет: InstalledSoft.
  if ($xml_contents = srr_inventagent_zipped_file_get_contents($zip_realpath, 'InstalledSoft (builtin)/data.xml')) {
    $xml = new SimpleXMLElement($xml_contents);
    $data = $xml->xpath('/packagedata');
    $data = current($data);
    foreach ($data as $soft_item) {
      $name = current($soft_item->DisplayName);
      $ver = current($soft_item->DisplayVersion);
      if (is_string($name)) {
        $name = trim($name);
        $ver = trim($ver);
        if (preg_match('/^Secret Net[ 0-9\.]*$/', $name)) {           
          $result['szi']['sn']['version'] = $ver;
          $result['szi']['sn']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        // В отчете 69882 нет просто Secret Net Studio.
        elseif ($name == 'Secret Net Studio' || $name == 'Secret Net Studio - Базовая защита') {
          $result['szi']['sns']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['sns']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        elseif (preg_match('/^ViPNet Client/ius', $name)) {
          $result['szi']['vn']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
          // Если версия випнета не извлеклась пакетом InventorySZI, то она извлекается из списка ПО.
          if (!isset($result['szi']['vn']['version']) || empty($result['szi']['vn']['version'])) {
            $result['szi']['vn']['version'] = trim(current($soft_item->DisplayVersion));
          }
        }
        elseif ($name == 'ViPNet IDS HS Agent') {
          $result['szi']['vnids_hs_agent']['version'] = $ver;
          $result['szi']['vnids_hs_agent']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));        
//          $lic_n = sup_sziflow_get_vnidshs_lic_from_file('/home/login1/IDS_HS_08122020.xlsx', str_replace('.rk.local', '', drupal_strtolower($result['meta']['host_name'])));
          $lic_n = sup_sziflow_get_vnidshs_lic_from_file('/var/spool/secmon/IDS_HS_08122020.xlsx', str_replace('.rk.local', '', drupal_strtolower($result['meta']['host_name'])));
          if ($lic_n) {
            $result['szi']['vnids_hs_agent']['license'] = $lic_n;
          }
        }
        elseif (preg_match('/^КриптоПро CSP/ius', $name)) {
          $result['szi']['cpcsp']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['cpcsp']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        elseif (preg_match('/^КриптоАРМ$/ius', $name)) {
          $result['szi']['crarm']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['crarm']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        } 
        elseif (preg_match('/Jinn-Client/ius', $name)) {
          $result['szi']['jinn_client']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['jinn_client']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        elseif (preg_match('/Континент\-АП/ius', $name)) {
          $result['szi']['continent_ap']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['continent_ap']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        elseif (preg_match('/Континент TLS Клиент/ius', $name)) {
          $result['szi']['continent_tls']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['continent_tls']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        elseif (preg_match('/^\s*Kaspersky Endpoint Security 10/ius', $name)) {
          $result['szi']['kes']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['kes']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        elseif (preg_match('/^\s*Kaspersky Endpoint Security для Windows/ius', $name)) {
          $result['szi']['kes']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['kes']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        elseif (preg_match('/^\s*Kaspersky Security для виртуальных сред.+Легкий агент.*/ius', $name)) {
          $result['szi']['ksvsla']['version'] = trim(current($soft_item->DisplayVersion));
          $result['szi']['ksvsla']['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
        elseif (preg_match('/^DallasLock8\.(.+)(K|C)/ius', $name, $m)) {
          $letter = $m[2];
          $letter_lc = drupal_strtolower($letter);
          $result['szi']['dl8'.$letter_lc]['version'] = trim(current($soft_item->DisplayVersion)) .' (8.'. $m[1] .''. $letter .')';
          $result['szi']['dl8'.$letter_lc]['install_date'] = sup_sziflow_install_date_to_timestamp(trim(current($soft_item->InstallDate)));
        }
      }
    }
  }

  if (FALSE && !empty($result['szi']['dl8k'])) { // by_evil!
    $dlock_info = file_get_contents('/var/spool/srr/dallaslock-map.txt');
    $dlock_info = explode("\n", $dlock_info);
    $curr_smid = drupal_strtolower($report->field_smid[LANGUAGE_NONE][0]['value']);
    foreach ($dlock_info as $dli) {
      list($lic, $zs, $user, $smid) = explode("\t", trim($dli));
      if ($smid && drupal_strtolower($smid) == $curr_smid) {
        $result['szi']['dl8k']['license'] = $lic;
        $result['szi']['dl8k']['_dl8k__zs'] = $zs;
      }
    }
  }
  if (FALSE && !empty($result['szi']['dl8c'])) { // by_evil!
    $dlock_info = file_get_contents('/var/spool/srr/dallaslock-map.txt');
    $dlock_info = explode("\n", $dlock_info);
    $curr_smid = drupal_strtolower($report->field_smid[LANGUAGE_NONE][0]['value']);
    foreach ($dlock_info as $dli) {
      list($lic, $zs, $user, $smid) = explode("\t", trim($dli));
      if ($smid && drupal_strtolower($smid) == $curr_smid) {
        $result['szi']['dl8c']['license'] = $lic;
        $result['szi']['dl8c']['_dl8c__zs'] = $zs;
      }
    }
  }

  // По KES и касперскомы для вирт. сред. данные учитываются только в серверных ОС.
  if (!$result['meta']['is_server_os']) {
    // Удаляется если не проставлена опция показа KES всегда.
    if (isset($result['szi']['kes']) && !$options['kes__never_hide']) {
      unset($result['szi']['kes']);
    }
    if (isset($result['szi']['ksvsla'])) {
      unset($result['szi']['ksvsla']);
    }
  }

  // На одном из отчетов было замечено, что КриптоАРМ не удаляет свои ключи реестра с серийником. 
  // Поэтому, если он отсутствует в списке ПО, то запись удаляется.
  // NID: 53552, 106adf5f-2bff-4513-be89-f5ec36178268.zip
  if (isset($result['szi']['crarm']) && !isset($result['szi']['crarm']['version'])) {
    unset($result['szi']['crarm']);
  }

  // Простановка типа СЗИ. Технически на будущее заложена возможность наличия нескольких 
  // одинаковых СЗИ на одном и том же компьютере. Пока тип проставляется таким способом.
  foreach ($result['szi'] as $k => $v) {
    $result['szi'][$k]['type'] = $k;
  }  

//  // @todo временно.
//  if ($xml_contents = srr_inventagent_zipped_file_get_contents($zip_realpath, 'meta.xml')) {
//    $xml = new SimpleXMLElement($xml_contents);
//    $data = $xml->xpath('/report');
//    $data = current($data);
//    $result['meta']['host_addr'] = trim($data->HostAddr);
//    $result['meta']['host_sticker'] = trim($data->HostSticker);
//    $result['meta']['comment'] = trim($data->Comment);
//    $result['meta']['host_name'] = trim($data->ComputerName);
//  }

  // Пакет: NWInfo.
  if ($xml_contents = @srr_inventagent_zipped_file_get_contents($zip_realpath, 'NWInfo (builtin)/data.xml')) {
    $xml = new SimpleXMLElement($xml_contents);
    $data = $xml->xpath('/packagedata');
    $data = current($data);
    $result['nwinfo'] = array();
    $keys = array('Caption', 'Description', 'DHCPEnabled', 'DHCPLeaseExpires', 'DHCPLeaseObtained', 'DHCPServer', 'DNSDomain', 'DNSEnabledForWINSResolution', 'DNSHostName', 'MACAddress', 'IPEnabled', 'DNSDomainSuffixSearchOrder', 'DNSServerSearchOrder', 'IPAddress', 'IPSubnet', 'DefaultIPGateway');
    foreach ($data as $if) {
      $if_parsed = array();
      foreach ($keys as $k) {
        $k_lc = drupal_strtolower($k);
        $v = current($if->{$k});
        $if_parsed[$k_lc] = is_string($v) ? trim($v) : '';
      }
      $result['nwinfo'][] = $if_parsed;
    }
  }

  return $result;
}


/** 
 * Возвращает сведения о сертификации по одному СЗИ. 
 * 
 * @param $szi
 *   Массив с данными по одному СЗИ. @see sup_sziflow_parse_sia_report().
 * @return 
 *   Строка со сведениями или FALSE;
 */
function sup_sziflow_szi_cert_info($szi) {
  // Secret Net Client.
  if ($szi['type'] == 'sn') {
    $mm = array(
      '7.6.*' => 'ФСТЭК России 2707 от 07.09.2012',      
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Secret Net Studio (ЦИТ).
  elseif ($szi['type'] == 'sns') {
    $mm = array(
      '8\.2\..*' => 'ФСТЭК России 3745 от 16.05.2017',
      '8\.4\..*' => 'ФСТЭК России 3745 от 16.05.2017',
      '8\.5\..*' => 'ФСТЭК России 3745 от 16.05.2017',
      '8\.6\..*' => 'ФСТЭК России 3745 от 16.05.2017',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // ViPNet Client.
  elseif ($szi['type'] == 'vn') {
    $mm = array(
      '3\.2.*' => 'ФСБ России СФ/124-2796 от 11.02.2016',
      '4\.3.*'    => 'ФСБ России СФ/124-3430 от 06.07.2018',
      '4\.5.*'    => 'ФСБ России СФ/124-4062 от 18.05.2021',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // КриптоПро CSP.
  elseif ($szi['type'] == 'cpcsp') {
    $mm = array(
      '3\.6.*' => 'ФСБ России СФ/114-2913 от 27.06.2016',
      '3\.9.*' => 'ФСБ России СФ/124-2538 от 15.01.2015',
      '4.*'   => 'ФСБ России СФ/114-3009 от 30.12.2016',
      '5.*' => 'ФСБ России СФ/114-3726 от 13.08.2019',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // КриптоПРО JCP.
  elseif ($szi['type'] == 'cpjcp') {
    $mm = array(
//      '1\.*' => 'ФСБ России СФ/114-2913 от 27.06.2016',
      '2\..*' => 'ФСБ России СФ/114-3546 от 04.12.2018',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // КриптоАРМ.
  elseif ($szi['type'] == 'crarm') {
    $mm = array(
      '5.*' => 'ФСБ России СФ/114-3120 от 03.05.2017',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // ViPnet IDS HS (агентское ПО).
  elseif ($szi['type'] == 'vnids_hs_agent') {
    $mm = array(
      '1\..*' => 'ФСТЭК России 3802 от 12.05.2017',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // JinnClient.
  elseif ($szi['type'] == 'jinn_client') {
    $mm = array(
      '1\..*' => 'ФСБ России СФ/121-3140 от 19.05.2017, СФ/123-3141 от 19.05.2017, СФ/121-3082 от 10.02.2017',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Континент АП.
  elseif ($szi['type'] == 'continent_ap') {
    $mm = array(
      '3\.5.*' => 'ФСБ России СФ/525–2045 от 10.02.2013, СФ/124–1812 от 08.06.2012, СФ/114–1811 от 08.06.2012',
      '3\.6.*' => 'ФСТЭК России 2648 от 31.05.2012',
      '3\.7.*' => 'ФСБ России СФ/114-2920 от 07.07.2016, СФ/124-3293 от 25.01.2018',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Континент TLS Клиент.
  elseif ($szi['type'] == 'continent_tls') {
    $mm = array(
      '1\..*' => 'ФСБ России СФ/114-2677 от 30.07.2015, СФ/124-2678 от 30.07.2015, СФ/124-3123 от 03.05.2017',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Kaspersky Endpoint Security.
  elseif ($szi['type'] == 'kes') {
    $mm = array(
      '10\..*' => 'ФСТЭК России 3025 от 25.11.2013',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Kaspersky Security для виртуальных сред - Легкий Агент.
  elseif ($szi['type'] == 'ksvsla') {
    $mm = array(
      '4\..*' => 'ФСТЭК России 3883 от 14.02.2018',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Dallas Lock 8.
  elseif ($szi['type'] == 'dl8k') {
    $mm = array(
      '8\..*' => 'ФСТЭК России 2720 от 25.09.2012',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }
  elseif ($szi['type'] == 'dl8c') {
    $mm = array(
      '8\..*' => 'ФСТЭК России 2945 от 16.08.2013',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Криптопро JCP.

  return FALSE;
}


/**
 * Возвращает сведения о сертифицированном дистрибутиве по одному СЗИ.
 *
 * @param $szi
 *   Массив с данными по одному СЗИ. @see sup_sziflow_parse_sia_report().
 * @return
 *   Строка со сведениями или FALSE;
 */
function sup_sziflow_szi_cert_distrib($szi) {
  // Secret Net Client.
  if ($szi['type'] == 'sn') {
    if (preg_match('/^7.6./ius', $szi['version'])) {
      if (isset($szi['license'])) {
        $license_lc = drupal_strtolower($szi['license']);
        if ($license_lc == 'tamv-07kf-buj2-00og-0fvp-00p2-000d') {
          return 'A6SR25BH, K274972';
        } 
        elseif ($license_lc == 'tamz-bv6c-btbt-0tff-0feq-00p2-000m') {
          return 'CWZLEW8H, Л243765';
        }
      }
      return FALSE;;      
    }
  }

  // Secret Net Studio.
  elseif ($szi['type'] == 'sns') {
    // Если версия 8.2,4,5 и лицензия - одна из ЦИТовских, тогда указывается наш дистрибутив.
    $cit_lics = array('11B839', '15248E', '1852C1', '197063', '1B3E01', '1EBBC8', '189F4DC');
    if (preg_match('/^8\.2\./ius', $szi['version']) && in_array($szi['license'], $cit_lics)) {
      return 'SV2GB7UT, M378034';
    } 
    elseif (preg_match('/^8\.4\./ius', $szi['version']) && in_array($szi['license'], $cit_lics)) {
      return '6D2FG8ZL, H284039';
    }
    elseif (preg_match('/^8\.5\./ius', $szi['version']) && in_array($szi['license'], $cit_lics)) {
      return 'Официальный электронный дистрибутив';
    }
    elseif (preg_match('/^8\.6\./ius', $szi['version']) && in_array($szi['license'], $cit_lics)) {
      return 'Официальный электронный дистрибутив';
    }
  }

  // ViPNet Client.
  elseif ($szi['type'] == 'vn') {
    $mm = array(
      '3\.2.*' => '637C-001571, 33571',
      '4.*'    => '782-004314',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // КриптоПро CSP.
  elseif ($szi['type'] == 'cpcsp') {
    $mm = array(
      '3\.6.*' => '385K-044927',
      '3\.9.*' => '385M-002375',
      '4\.0\.9842' => '385C1-005456',
      '4\.0\.9963' => '385C1-015901',      
      '4.*'   => 'Официальный электронный дистрибутив',
      '5.*' => 'Официальный электронный дистрибутив',		
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // КриптоПро JCP.
  elseif ($szi['type'] == 'cpjcp') {
    return 'Официальный электронный дистрибутив';
  }

  // КриптоАРМ.
  elseif ($szi['type'] == 'crarm') {
    $mm = array(
      '5.*' => 'Официальный электронный дистрибутив',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // ViPNet IDS HS Agent
  elseif ($szi['type'] == 'vnids_hs_agent') {
    $mm = array(
      '1\..*' => '00466, М813467',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Kaspersky Endpoint Security.
  elseif ($szi['type'] == 'kes') {
    $mm = array(
      '10\..*' => 'СМП8069-15948, M702386',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Kaspersky Security для виртуальных сред - легкий агент.
  elseif ($szi['type'] == 'ksvsla') {
    $mm = array(
      '4\..*' => 'СМП8072-0686,К1462888',
    );
    foreach ($mm as $ver_pattern => $info) {
      if (preg_match('/^'. $ver_pattern .'$/ius', $szi['version'])) {
        return $info;
      }
    }
  }

  // Dallas Lock.
  elseif ($szi['type'] == 'dl8k') {
    if (isset($szi['_dl8k__zs'])) {
      return $szi['_dl8k__zs'];
    }
  }

  // Dallas Lock.
  elseif ($szi['type'] == 'dl8c') {
    if (isset($szi['_dl8c__zs'])) {
      return $szi['_dl8c__zs'];
    }
  }

  return FALSE;

  
  /*end*/
}


/**
 * Преобразует виндовую Install Date (в формате ГГГГММДД) в штамп времени.
 * 
 * @param $install_date
 *   Дата установки в виндовом формате.
 * @return
 *   Дата установки в формате штампа времени или FALSE в случае ошибки.
 */
function sup_sziflow_install_date_to_timestamp($install_date) {
  if (preg_match('/^(\d{4})(\d{2})(\d{2})$/', $install_date, $m)) {
    return mktime(0, 0, 0, $m[2], $m[3], $m[1]);
  }
  return FALSE;
}



/* PDF */

/**
 * Преобразование ноды типа akt_szi в PDF-файл.
 *
 * @param $akt_node
 *   Полностью загруженная нода-акт.
 * @param $output_uri
 *   URI, по которому будет сохранен созданный PDF. Если файл существует, он будет заменен.
 * @return
 *   TRUE/FALSE в зависимости от успеха преобразования.
 */
 
 /* 03052023 lau */
 // ИНН из БД
	function sup_sziflow_get_inn($id){
	return db_query("SELECT field_org_inn_value FROM {field_revision_field_org_inn} WHERE entity_id = :value", array(':value' => $id))->fetchField();
}


// Принадлежость к inrklocal
	function isrklocal($host){
		if ($host == '1')
		{
			return "Да";
		}
		else
		{
			return "Нет";
		}
	}
 
function sup_sziflow_convert_akt_szi_to_pdf($akt_node, $output_uri) {  
  $content = '';
  $meta = array();

  // Преобразование списка представленных на компьютере СЗИ в массив и их группировка по времени 
  // установки.
  $presented_szi = array('all' => array(), 'not-changed' => array(), 'installed-now' => array());
  $presented_szi['all'] = drupal_json_decode($akt_node->field_akt_szi_presented[LANGUAGE_NONE][0]['value']);
  foreach ($presented_szi['all'] as $v) {
    if ($v['install_mode'] === 'not-changed' || $v['install_mode'] === 'installed-now') {
      $presented_szi[$v['install_mode']][] = $v;
    }
  }

  // Преобразование списка удаленных СЗИ в массив. Из списка удаленных исключаются те, которые
  // были отмечены, что они были удалены не сейчас.
  $removed_szi = drupal_json_decode($akt_node->field_akt_szi_removed[LANGUAGE_NONE][0]['value']);
  foreach ($removed_szi as $k => $szi) {
    if ($szi['remove_mode'] != 'removed-now') {
      unset($removed_szi[$k]);
    }
  }

  // Подготовка метаданных.
  
  // Добавление нового поля - 03052023 lau
  //$meta['inn'] = $akt_node->field_org_inn['und'][0]['value']; // here 2
  
  $meta['date'] = date('d.m.Y', $akt_node->field_akt_szi_date[LANGUAGE_NONE][0]['value']);
  $meta['no'] = $akt_node->field_akt_szi_no[LANGUAGE_NONE][0]['value'];
  $meta['smid'] = $akt_node->field_smid[LANGUAGE_NONE][0]['value'];
  $meta['org'] = sup_sziflow_get_node_title($akt_node->field_org[LANGUAGE_NONE][0]['target_id']);
  // Данные о сотруднике, формирующем акт.
  $installer = entity_load_single('user', $akt_node->uid);
  $meta['installer_fio'] = isset($installer->field_user_fio[LANGUAGE_NONE][0]['value'])
    ? $installer->field_user_fio[LANGUAGE_NONE][0]['value']
    : '';
  $meta['installer_title'] = isset($installer->field_user_title[LANGUAGE_NONE][0]['value'])
    ? $installer->field_user_title[LANGUAGE_NONE][0]['value']
    : '';
  $meta['installer_org'] = isset($installer->field_org[LANGUAGE_NONE][0]['target_id'])
    ? sup_sziflow_get_node_title($installer->field_org[LANGUAGE_NONE][0]['target_id'])
    : '';
  // Данные из ноды-акта.
  $map_akt_value = array(
    'host_user' => 'field_akt_szi_user',
    'host_inv'  => 'field_akt_szi_inv',
    'host_addr' => 'field_akt_szi_host_addr',
    'host_type' => 'field_akt_szi_host_type',
  ); 
  foreach ($map_akt_value as $meta_k => $akt_field) {
    $meta[$meta_k] = isset($akt_node->{$akt_field}[LANGUAGE_NONE][0]['value'])
      ? $akt_node->{$akt_field}[LANGUAGE_NONE][0]['value']
      : '';
  }
  $type_field = field_info_field('field_akt_szi_host_type');
  $type_values = list_allowed_values($type_field);  
  if (!empty($meta['host_type'])) {
    $meta['host_type'] = $type_values[$meta['host_type']];
  }


  // Формирование корректного заголовка.
  $meta['title'] = (!empty($presented_szi['installed-now']) || !empty($removed_szi))
    ? 'Акт об изменении состава СЗИ'
    : 'Акт обследования состава СЗИ';
  $content .= '<h1>'. format_string('Акт №!no от !date<br/>!name', array(
     '!no' => $meta['no'],
     '!date' => $meta['date'], 
     '!name' => (!empty($presented_szi['installed-now']) || !empty($removed_szi)) 
       ? 'об изменении состава средств защиты информации' 
       : 'об обследовании состава средств защиты информации'
  )) .'</h1>';

  $meta['works'] = array('sia_report' => 'Обследование компьютера');
  if (!empty($presented_szi['installed-now'])) {
    $meta['works']['install-szi'] = 'Установка и настройка средств защиты информации';
  }
  if (!empty($removed_szi)) {
    $meta['works']['remove-szi'] = 'Удаление средств защиты информации';
  }

  // Номер раздела акта. 
  $section_no = 1;

  $szi_naming = array(
    'vn' => 'ViPNet Client',
    'sn' => 'Secret Net',
    'cpcsp' => 'КриптоПРО CSP',
  );
  
  $content .= '<div style="margin: 20px 0 10px; text-align: justify; text-indent: 35px">'; 
  $content .= format_string('Работник @installer-org @installer-fio настоящим актом подтверждает, что им проведены работы согласно Таблице 1 на компьютере, сведения о котором указаны в Таблице 2.', array(
    '@installer-org' => $meta['installer_org'],
    '@installer-fio' => $meta['installer_fio'],
    '@installer-title' => $meta['installer_title'],
  ));
  $content .= '</div>';

  $block = '<caption>Таблица '. ($section_no++) .'. Сведения о работах</caption>';
  $block .= '<table cellspacing=0 cellpadding=0>';
  $block .= '<tbody>';
  $block .= '<tr>';
  $block .= '<th style="text-align: left; width: 25%">Перечень работ</th>';
  $block .= '<td>'. implode('<br/>', $meta['works']) .'</td>';
  $block .= '</tr>';
  $block .= '<tr>';
  $block .= '<th style="text-align: left; width: 25%">Комментарий</th>';
  $block .= '<td>'. check_plain($akt_node->field_akt_szi_comment[LANGUAGE_NONE][0]['value']) .'</td>';
  $block .= '</tr>';
  $block .= '</tbody>';
  $block .= '</table>';
  $content .= $block;

  $block = '<caption>Таблица '. ($section_no++) .'. Сведения о компьютере</caption>';
  $block .= '<table cellspacing=0 cellpadding=0>';
  $block .= '<tbody>';
  $block .= '<tr>';
  $block .= '<th style="width:25%; text-align: left">Организация</th>';
  $block .= '<td colspan=3>'. check_plain($meta['org']) .'</td>';
  $block .= '</tr>';
  
  /* 03052023 lau ИНН организации */
  $block .= '<tr>';
  $block .= '<th style="text-align: left">ИНН</th>';
  //$block .= '<td colspan=3>'. check_plain($meta['host_addr']) .'</td>'; // было, оставил для примера
  //$block .= '<td colspan=3>' . check_plain($meta['inn']) . '</td>'; // here 1
  $block .= '<td colspan=3>' . check_plain(sup_sziflow_get_inn($akt_node->field_org['und'][0]['target_id'])) . '</td>'; // here 1
  $block .= '</tr>';
  /*  */
  
  
  if ($akt_node->field_akt_szi_host_type[LANGUAGE_NONE][0]['value'] != 'vm') {
    $block .= '<tr>';
    $block .= '<th style="text-align: left">Пользователь</th>';
    $block .= '<td colspan=3>'. check_plain($meta['host_user']) .'</td>';
    $block .= '</tr>';
  }
  
  
  
  $block .= '<tr>';
  $block .= '<th style="text-align: left">Расположение</th>';
  $block .= '<td colspan=3>'. check_plain($meta['host_addr']) .'</td>';
  $block .= '</tr>';
  $block .= '<tr>';
  $block .= '<th style="text-align: left">Тип компьютера, ОС</th>';
  $block .= '<td colspan=1>'. check_plain($meta['host_type']) .'</td>';
  $block .= '<td colspan=2>'. check_plain($akt_node->field_akt_szi_host_os[LANGUAGE_NONE][0]['value']) .'</td>';
  $block .= '</tr>';
  
  if ($akt_node->field_akt_szi_host_type[LANGUAGE_NONE][0]['value'] != 'vm') {
    $block .= '<tr>';
    $block .= '<th style="text-align: left">Инвентарный номер</th>';
    $block .= '<td colspan=3>'. check_plain($meta['host_inv']) .'</td>';
    $block .= '</tr>'; 
  }
  /* 03052023 lau */
  
  /*
  function isrklocal(){
		if ($node->field_hostref_inrklocal['und'][0]['value'] == '1')
		{
			return "Да";
		}
		else
		{
			return "Нет";
		}
	}
  */
  $block .= '<tr>';
  $block .= '<th style="text-align: left">Компьютер подключен к домену rk.local?</th>';
  $block .= '<td colspan=3>' . isrklocal($node->field_hostref_inrklocal['und'][0]['value']);
  $block .= '</td></tr>';
  */
  
  
  
  /* end */
  
  $block .= '<tr>';
  $block .= '<th style="text-align: left">SMID, имя, отчет SIA</th>';
  $block .= '<td>'. check_plain($meta['smid']) .'</td>';
  $block .= '<td>'. check_plain($akt_node->field_akt_szi_host_name[LANGUAGE_NONE][0]['value']) .'</td>';
  $block .= '<td>'. $akt_node->field_akt_szi_siarep[LANGUAGE_NONE][0]['target_id'] .'</td>';
  $block .= '</tr>';
  
  
  if ($akt_node->field_akt_szi_host_type[LANGUAGE_NONE][0]['value'] != 'vm') {
    $block .= '<tr>';
    $block .= '<th style="text-align: left">Опечатано</th>';
    $block .= '<td colspan=3>'. check_plain($akt_node->field_akt_szi_host_sticker[LANGUAGE_NONE][0]['value']) .'</td>';
    $block .= '</tr>'; 
  }
  $block .= '</tbody>';
  $block .= '</table>';
  $content .= $block;

  // Перечень СЗИ, которые были установлены сейчас.
  if (!empty($presented_szi['installed-now'])) {
    $block  = '<caption>Таблица '. ($section_no++) .'. Установленные (обновленные) СЗИ</caption>';
    $block .= '<table style="border-collapse:collapse; border-spacing: 0; border: 1px solid black;">';
    $block .= '<thead><tr>';
    $block .= '<th>Наименование</th>';
    $block .= '<th>Номер лицензии СЗИ</th>';
    $block .= '<th>Тип СЗИ</th>';
    $block .= '<th>Сведения о сертификации</th>';
    $block .= '<th>Установочный комплект, знак соответствия</th>';
    $block .= '</tr></thead>';
    $block .= '<tbody>';
    foreach ($presented_szi['installed-now'] as $szi) {
      $display = sup_sziflow_display_szi($szi);
      $block .= '<tr>';
      $block .= '<td>'. $display['display_name'] .'</td>';
      $block .= '<td>'. $display['license']      .'</td>';
      $block .= '<td>'. $display['szi_type']     .'</td>';
      $block .= '<td>'. $szi['cert_info']        .'</td>';
      $block .= '<td>'. $szi['cert_distrib']     .'</td>';
      $block .= '</tr>';
    }
    $block .= '</tbody>';
    $block .= '</table>';
    $content .= $block;
  }

  // Перечень СЗИ, которые были установлены ранее.
  if (!empty($presented_szi['not-changed'])) {
    $block  = '';
    $block .= empty($presented_szi['not-changed'])
      ? '<caption>Таблица '. ($section_no++) .'. СЗИ, имеющиеся на компьютерe</caption>'
      : '<caption>Таблица '. ($section_no++) .'. СЗИ, установленые на компьютер ранее</caption>';
    $block .= '<table width="100%" style="border-collapse:collapse; border-spacing: 0; border: 1px solid black;">';
    $block .= '<thead>';
    $block .= '<tr>';
    $block .= '<th>Наименование СЗИ</th>';
    $block .= '<th>Номер лицензии СЗИ</th>';
    $block .= '<th>Тип СЗИ</th>';
    $block .= '</tr>';
    $block .= '</thead>';
    $block .= '<tbody>';
    foreach ($presented_szi['not-changed'] as $szi) {
      $display = sup_sziflow_display_szi($szi);
      $block .= '<tr>';
      $block .= '<td>'. $display['display_name'] .'</td>';
      $block .= '<td>'. $display['license']      .'</td>';
      $block .= '<td>'. $display['szi_type']     .'</td>';
      $block .= '</tr>';
    }
    $block .= '</tbody>';
    $block .= '</table>';
    $content .= $block;
  } 

  // Перечень удаленных СЗИ.
  if (!empty($removed_szi)) {
    $block = '';
    $block .= '<caption>Таблица '. ($section_no++) .'. Удаленные СЗИ</caption>';
    $block .= '<table width="100%" style="border-collapse:collapse; border-spacing: 0; border: 1px solid black;">';
    $block .= '<thead>';
    $block .= '<tr>';
    $block .= '<th>Наименование</th>';
    $block .= '<th>Сведения о лицензировании</th>';
    $block .= '<th>Тип СЗИ</th>';
    $block .= '</tr>';
    $block .= '</thead>';
    $block .= '<tbody>';
    foreach ($removed_szi as $szi) {
      $display = sup_sziflow_display_szi($szi);
      $block .= '<tr>';
      $block .= '<td>'. $display['display_name'] .'</td>';
      $block .= '<td>'. $display['license']      .'</td>';
      $block .= '<td>'. $display['szi_type']     .'</td>';
      $block .= '</tr>';
    }
    $block .= '</tbody>';
    $block .= '</table>';
    $content .= $block;
  }


  if (!empty($presented_szi['installed-now'])) {
    $content .= '<div style="margin: 20px 0 0; text-align: justify; text-indent: 35px">';
    $content .= format_string('Установка и настройка проводилась в соответствии с эксплуатационной документацией на СЗИ. По окончании установки и настройки СЗИ проведены тестовые испытания, в ходе которых нарушения работоспособности компьютера и конфликтов в операционной системе выявлено не было.');
    $content .= '</div>';
    $content .= '<div style="text-align: justify; text-indent: 35px">';
    $content .= format_string('Работник @installer-org делает заключение о готовности установленных (обновленных) СЗИ к эксплуатации в целях обеспечения защиты информации и о готовности пользователя (администратора) к самостоятельной работе с СЗИ.', array(
      '@installer-org' => $meta['installer_org'],
    ));
    $content .= '</div>';
  }
 

  $signer_cert = entity_load_single('node', $akt_node->field_akt_szi_sig_cert[LANGUAGE_NONE][0]['target_id']);
  $content .= '<div style="width:70%; margin: 60px 0 20px; border: 2px solid black; padding:10px; font-size: 9pt; page-break-inside:avoid;">';
  $content .= '<div style="margin-bottom: 7px">ПОДПИСАНО (электронная подпись без штампа времени)</div>';
  $content .= '<b style="font-size: 10pt">'. $signer_cert->field_cert_subj_sn[LANGUAGE_NONE][0]['value'] .' '. $signer_cert->field_cert_subj_g[LANGUAGE_NONE][0]['value'] .'</b><br/>';
  $content .= $signer_cert->field_cert_subj_cn[LANGUAGE_NONE][0]['value'] .'</b><br/>';
  $content .= 'Издатель: '. $signer_cert->field_cert_iss_cn[LANGUAGE_NONE][0]['value'] .'<br/>';
  $content .= 'Серийный номер: '. $signer_cert->field_cert_serial[LANGUAGE_NONE][0]['value'] .'<br/>';
  $content .= 'Срок действия: '. date('d.m.Y H:i:s', $signer_cert->field_cert_notbefore[LANGUAGE_NONE][0]['value']) .'&mdash;'. date('d.m.Y H:i:s', $signer_cert->field_cert_notafter[LANGUAGE_NONE][0]['value']) .' (UTC)<br/>';
  $content .= '</div>';  

  $assurer_cert = entity_load_single('node', $akt_node->field_akt_szi_assure_cert[LANGUAGE_NONE][0]['target_id']);
  $content .= '<div style="width:70%; border: 2px solid black; padding:10px; font-size: 9pt; page-break-inside:avoid;">';
  $content .= '<div style="margin-bottom: 7px">ПОДПИСЬ ЗАВЕРЕНА (электронная подпись со штампом времени)</div>';
  $content .= '<b style="font-size: 10pt">'. $assurer_cert->field_cert_subj_sn[LANGUAGE_NONE][0]['value'] .' '. $assurer_cert->field_cert_subj_g[LANGUAGE_NONE][0]['value'] .'</b><br/>';
  $content .= $assurer_cert->field_cert_subj_cn[LANGUAGE_NONE][0]['value'] .'</b><br/>';
  $content .= 'Издатель: '. $assurer_cert->field_cert_iss_cn[LANGUAGE_NONE][0]['value'] .'<br/>';
  $content .= 'Серийный номер: '. $assurer_cert->field_cert_serial[LANGUAGE_NONE][0]['value'] .'<br/>';
  $content .= 'Срок действия: '. date('d.m.Y H:i:s', $assurer_cert->field_cert_notbefore[LANGUAGE_NONE][0]['value']) .'&mdash;'. date('d.m.Y H:i:s', $assurer_cert->field_cert_notafter[LANGUAGE_NONE][0]['value']) .' (UTC)<br/>';
  $content .= '</div>';


  $content = '
  <html>
  <head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
  <style> 
    * { font-family: "Times New Roman"; } 
    body { font-size: 10pt; font-family: "Times New Roman"; }
    p { margin: 0; } 
    table { width: 100%; border-collapse: collapse; border: 1px solid black; border-spacing: 0; margin-bottom: 20px; }
    td, th { border: 1px solid black; padding: 2px 4px; }
    h1 { font-size: 13pt; text-align: center; }
    caption { margin-bottom: 7px; text-align: right; }
  </style>
  </head>
  <body>'.  $content .'</body>
  </html>';

  // Генерация PDF.
  require_once DRUPAL_ROOT . '/sites/all/libraries/mpdf/mpdf60/mpdf.php';
  $mpdf = new mPDF('utf-8', 'A4', 0, '', 20, 8, 10, 10, 0, 0);
  $mpdf->WriteHTML($content);

  // Сохранение PDF в файл.
  $mpdf->Output(drupal_realpath($output_uri), 'F');

  return TRUE;
}


function sup_sziflow_display_szi($szi) {
  switch ($szi['type']) {
    case 'sn':
      return array(
        'display_name' => 'Secret Net Client '. $szi['version'],
        'license' => !empty($szi['license']) ? $szi['license'] : '',
        'szi_type' => 'Программный',
      );

    case 'sns':
//      $license = '';
//      if (preg_match('/^8\.2\./ius', $szi['version'])) { 
//        $license == '11B839';
//      }
//      elseif (preg_match('/^8\.4\./ius', $szi['version'])) {
//        $license == '15248E';
//      }
      return array(
        'display_name' => 'Secret Net Studio '. $szi['version'],
        'license' => !empty($szi['license']) ? $szi['license'] : '',
        'szi_type' => 'Программный',
      );

    case 'vn':
      return array(
        'display_name' => 'ViPNet Client '. $szi['version'],
        'license' => 'Номер АП: '. $szi['_vn__apid'],
        'szi_type' => 'Программный',
      );

    case 'vnids_hs_agent':
      return array(
        'display_name' => 'ViPNet IDS HS (агентское ПО) '. $szi['version'],
        'license' => !empty($szi['license']) ? $szi['license'] : '1331668/2/1-IDSHS',
        'szi_type' => 'Программный',
      );

    case 'cpcsp':
      $szi['license'] = drupal_strtolower(!empty($szi['license']) ? $szi['license'] : '');
      $demo_keys = array('39390z0037ea3yggrqede6lpz', '3636740030empwpc6617nt3dy', '4040eg0037ek8r3c6k4uhcxqg', '5050010037elqf5h28km8e6ba');
      $sn = drupal_strtoupper(preg_replace('/^(.{5})(.{5})(.{5})(.{5})(.+)$/ius', '$1-$2-$3-$4-$5', $szi['license']));
      $sn .= in_array($szi['license'], $demo_keys) ? ' (демо)' : '';
      return array(
        'display_name' => 'КриптоПро CSP '. $szi['version'],
        'license' => $sn,
        'szi_type' => 'Программный',
      );

    case 'crarm':
      $demo_keys = array('td5qcqhwdkgdrqwfckhcrgmqhtfmxmcdcvk');
      $sn = drupal_strtoupper(preg_replace('/^(.{5})(.{5})(.{5})(.{5})(.{5})(.{5})(.+)$/ius', '$1-$2-$3-$4-$5-$6-$7', $szi['license']));      
      $sn .= in_array($szi['license'], $demo_keys) ? ' (демо)' : '';
      return array(
        'display_name' => 'КриптоАРМ Стандарт '. $szi['version'],
        'license' => $sn, 
        'szi_type' => 'Программный',
      );

    case 'jinn_client':
      return array(
        'display_name' => 'Jinn-Client '. $szi['version'],
        'license' => '',
        'szi_type' => 'Программный',
      );

    case 'continent_ap':
      return array(
        'display_name' => 'Континент-АП '. $szi['version'],
        'license' => '',
        'szi_type' => 'Программный',
      );

    case 'kes':
      return array(
        'display_name' => 'Kaspersky Endpoint Security '. $szi['version'],
        'license' => '1C1C-000451-5712947A',
        'szi_type' => 'Программный',
      );

    case 'ksvsla':
      return array(
        'display_name' => 'Kaspersky Security для виртуальных сред - Легкий агент '. $szi['version'],
        'license' => '1C1C-0005D3-572A1E24',
        'szi_type' => 'Программный',
      );

    case 'dl8k':
    case 'dl8c':
      return array(
        'display_name' => 'Dallas Lock '. $szi['version'],
        'license' => $szi['license'],
        'szi_type' => 'Программный',
      );

    case 'continent_tls':
      return array(
        'display_name' => 'Континент TLS Клиент '. $szi['version'],
        'license' => '',
        'szi_type' => 'Программный',
      );

    case 'cpjcp':
      return array(
        'display_name' => 'КриптоПро JCP '. $szi['version'],
        'license' => $szi['license'],
        'szi_type' => 'Программный',
      );

  }
  return FALSE;
}


function sup_sziflow_tmp_test_pdf($nid) {
  $node = entity_load_single('node', $nid);

  // Формирование номера акта.
  $node->field_akt_szi_no[LANGUAGE_NONE][0]['value'] = format_string('ПБ-!nid/!year', array(
    '!nid' => $node->nid,
    '!year' => date('Y', $node->field_akt_szi_date[LANGUAGE_NONE][0]['value']),
  ));

  // Создание PDF-файла и прикрепление его к ноде.
  module_load_include('inc', 'sup_sziflow', 'includes/sup_sziflow.lib');
  $pdf_dir = format_string('private://akt-szi/!nid', array('!nid' => $node->nid));
  file_prepare_directory($pdf_dir, FILE_CREATE_DIRECTORY);
  $pdf_uri = format_string('!dir/akt-szi-!nid.pdf', array('!nid' => $node->nid, '!dir' => $pdf_dir));
  sup_sziflow_convert_akt_szi_to_pdf($node, $pdf_uri);
//  $file = new StdClass();
//  $file->filename = basename($pdf_uri);
//  $file->uri = $pdf_uri;
//  $file->status = FILE_STATUS_PERMANENT;
//  $file->filemime = file_get_mimetype($pdf_uri);
//  $file = file_save($file);
//  $node->field_akt_szi_pdf[LANGUAGE_NONE][] = array(
//    'fid' => $file->fid,
//    'display' => 1,
//  );

  // Акт сохраняется в статусе "Подготовлен".
//  $node->field_akt_szi_status[LANGUAGE_NONE][0]['value'] = 'prepared';
//  entity_save('node', $node);


}


/**
 * Получаен номер лицензии ViPNet IDS HS из файла по имени компьютера.
 */
function sup_sziflow_get_vnidshs_lic_from_file($filepath, $hostname) {
  static $cache = array();

  if (!isset($cache[$filepath])) {
//    require_once '/var/www/secmon/www/drupal/sites/all/libraries/PHPExcel-1.8.1/vendor/autoload.php';
    require_once '/var/www/security/www/drupal-root/drupal/sites/all/libraries/PHPExcel-1.8.1/vendor/autoload.php';
    $inputFileType = 'Excel2007';
    $objReader = PHPExcel_IOFactory::createReader($inputFileType);
    $objPHPExcel = $objReader->load($filepath);
    $objPHPExcel->setActiveSheetIndexByName('Лист1');
    $sheet_data = $objPHPExcel->getActiveSheet()->toArray(NULL, TRUE, TRUE, TRUE);

    $map = array();
    foreach ($sheet_data as $row) {
      $host = drupal_strtolower($row['A']);
      $lic = $row['F'];
      $map[$host] = $lic;
    }

    $cache[$filepath] = $map;
  }

  return isset($cache[$filepath][$hostname])
    ? $cache[$filepath][$hostname]
    : NULL;
}

