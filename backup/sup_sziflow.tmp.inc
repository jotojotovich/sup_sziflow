<?php

/**
 * @file
 * Временный функционал модуля sup_sziflow.
 */


/**
 * Формирование PDF-заключения из результатов теста СКЗИ.
 *
 * @param $doc_node
 *   Полностью загруженная нода с документом-заключением.
 * @param $output_uri
 *   URI, по которому будет сохранен созданный PDF. Если файл существует, он будет заменен.
 * @return
 *   TRUE/FALSE в зависимости от успеха преобразования.
 */
function sup_sziflow_convert_skzi_zakl_to_pdf($doc_node, $output_uri) {  
  $content = '';
  $meta = array();

  $result_id = $doc_node->field_skzizakl_result_id[LANGUAGE_NONE][0]['value'];
  $result = sup_sziflow_load_skzitest_result($result_id);
  $quiz = node_load($result->nid, $result->vid);
  $questions = _quiz_get_answers($quiz, $result_id);
  $score = quiz_calculate_score($quiz, $result_id);
  $summary = _quiz_get_summary_text($quiz, $score);
  $account = entity_load_single('user', $result->uid);


  $content .= '<h1>'. format_string('Заключение №@no от !date<br/>о допуске пользователя к самостоятельной работе<br/>со средствами криптографической защиты', array(
     '@no' => $doc_node->field_skzizakl_no[LANGUAGE_NONE][0]['value'],
     '!date' => date('d.m.Y', $doc_node->field_skzizakl_date[LANGUAGE_NONE][0]['value']), 
  )) .'</h1>';
  
  $content .= '<div style="margin: 20px 0 10px; text-align: justify; text-indent: 35px">';
  $content .= format_string('Во исполнение требований «Инструкции об организации и обеспечении безопасности хранения, обработки и передачи по каналам связи с использованием средств криптографической защиты информации с ограниченным доступом, не содержащей сведений, составляющих государственную тайну», утвержденной приказом ФАПСИ от 13 июня 2001 г. № 152, пользователь прошел обучение правилам работы со средствами криптографической защиты информации (далее – СКЗИ) и показал следующие результаты в рамках принятого зачета:');
  $content .= '</div>';

  $content .= '<ul style="margin: 0; text-align: justify; text-indent: 0; margin:0; padding: 0; list-style-type:none;">';
  $content .= '<li><b>Организация: </b> '. check_plain(sup_sziflow_get_node_title($doc_node->field_org[LANGUAGE_NONE][0]['target_id'])).'</li>';
  $content .= '<li><b>Должность пользователя: </b> '. check_plain($doc_node->field_skzizakl_user_title[LANGUAGE_NONE][0]['value']) .'</li>';
  $content .= '<li><b>ФИО пользователя: </b> '. check_plain($doc_node->field_skzizakl_user_fio[LANGUAGE_NONE][0]['value']) .'</li>';
  $content .= '<li><b>Адрес электронной почты: </b> '. $account->mail .'</li>';
  $content .= '<li><b>Способ проведения зачета: </b> Тестирование на портале по безопасности ГАУ РК «ЦИТ» security.rkomi.ru</li>';
  $content .= '<li><b>Дата проведения зачета: </b> '. date("d.m.Y", $result->time_end) .'</li>';
  $content .= '<li><b>Результат: </b> '. $score['numeric_score'] .' из '. $score['possible_score'] .' баллов (ID результата: '. $result->result_id .')</li>';
  $content .= '</ul>';

  $content .= '<div style="margin: 10px 0; text-align: justify; text-indent: 35px">';
  $content .= format_string('Я, <em>@fio</em>, подтверждаю, что <em>@date г.</em> самостоятельно прошел(ла) тестирование на знание правил работы с СКЗИ на портале по безопасности ГАУ РК «ЦИТ» security.rkomi.ru, ознакомлен(а) с требованиями нормативных документов в области СКЗИ и «Инструкцией пользователя СКЗИ». Их содержание мне понятно, обязуюсь руководствоваться данными требованиями при работе с СКЗИ.', array(
    '@fio' => check_plain($doc_node->field_skzizakl_user_fio[LANGUAGE_NONE][0]['value']),
    '@date' => date("d.m.Y", $result->time_end),
  ));
  $content .= '</div>';

  $content .= '<div style="margin: 15px 0 0; text-align: right">';
  $content .= '__________________________ / ____________________________________<br/><small><i>подпись&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;расшифровка&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</i></small>';
  $content .= '</div>';

  $content .= '<div style="margin: 10px 0; text-align: justify; text-indent: 35px">';
  $content .= format_string('На основании результатов принятого зачета комиссия сотрудников органа криптографической защиты ГАУ РК «ЦИТ» дает заключение о том, что указанный пользователь прошел тестирование на знание правил работы с СКЗИ на портале по безопасности ГАУ РК «ЦИТ» security.rkomi.ru и готов к самостоятельной работе с СКЗИ.');
  $content .= '</div>';

  $signer_cert = entity_load_single('node', $doc_node->field_skzizakl_sig_cert[LANGUAGE_NONE][0]['target_id']);
  $content .= '<div style="width:70%; margin: 60px 0 20px; border: 2px solid black; padding:10px; font-size: 9pt; page-break-inside:avoid;">';
  $content .= '<div style="margin-bottom: 7px">ПОДПИСАНО (электронная подпись со штампом времени)</div>';
  $content .= '<b style="font-size: 10pt">'. $signer_cert->field_cert_subj_sn[LANGUAGE_NONE][0]['value'] .' '. $signer_cert->field_cert_subj_g[LANGUAGE_NONE][0]['value'] .'</b><br/>';
  $content .= $signer_cert->field_cert_subj_cn[LANGUAGE_NONE][0]['value'] .'</b><br/>';
  $content .= 'Издатель: '. $signer_cert->field_cert_iss_cn[LANGUAGE_NONE][0]['value'] .'<br/>';
  $content .= 'Серийный номер: '. $signer_cert->field_cert_serial[LANGUAGE_NONE][0]['value'] .'<br/>';
  $content .= 'Срок действия: '. date('d.m.Y H:i:s', $signer_cert->field_cert_notbefore[LANGUAGE_NONE][0]['value']) .'&mdash;'. date('d.m.Y H:i:s', $signer_cert->field_cert_notafter[LANGUAGE_NONE][0]['value']) .' (UTC)<br/>';
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



function sup_sziflow_tmp_zakl() {
  $node = new StdClass();
  $node->nid = 1112;
  
  module_load_include('inc', 'sup_sziflow', 'includes/sup_sziflow.lib');
  $pdf_dir = format_string('public://zakl-tmp', array('!nid' => $node->nid));
  file_prepare_directory($pdf_dir, FILE_CREATE_DIRECTORY);
  $pdf_uri = format_string('!dir/zakl-!nid.pdf', array('!nid' => $node->nid, '!dir' => $pdf_dir));
  sup_sziflow_convert_skzi_zakl_to_pdf($node, $pdf_uri);

}


function sup_sziflow_tmp_skzicab_hoststest() {
  module_load_include('inc', 'sup_sziflow', 'includes/sup_sziflow.lib');  
  $org_nid = arg(1);
//  $org_nid = 1290;
  $org_name = srr_core_node_get_single_field_value($org_nid, '', 'title');
//  $org_name = 'СЛУЖБА РЕСПУБЛИКИ КОМИ СТРОЙЖИЛТЕХНАДЗОРА';

  // Загрузка хостов с секмона.
  $hosts = sup_sziflow_skzicab_get_hosts_from_secmon($org_name);
  //@todo Убрать.
  foreach ($hosts as $k => $v) {
    $hosts[$k]['owner_fio'] = preg_replace('/^(.+)\s+\(.+\)$/ius', '$1', $v['owner_fio']);
  }

  // Загрузка хостов, которые есть только на портале.
  $hosts_suponly = array();
//  $skip_hosts = array_keys($hosts);
//  $skip_hosts[] = '';
  $akt_query = new EntityFieldQueryExtraFields();
  $akt_result = $akt_query
    ->entityCondition('entity_type', 'node')
    ->entityCondition('bundle', 'akt_szi')  
    ->propertyCondition('status', NODE_PUBLISHED)
    ->fieldCondition('field_org', 'target_id', $org_nid)
    ->fieldCondition('field_akt_szi_status', 'value', 'assured')
//    ->fieldCondition('field_sia_report_hostname', 'value', $skip_hosts, 'NOT IN')
//    ->fieldCondition('field_host_inv_time', 'value', time()-360*24*3600, '>')
    ->propertyOrderBy('nid', 'desc')
    ->addExtraField('field_akt_szi_siarep', 'target_id', 'target_id')
    ->addExtraField('field_akt_szi_date', 'value', 'value')
    ->addExtraField('', 'title', 'title', 'node')
    ->addTag('DANGEROUS_ACCESS_CHECK_OPT_OUT')
    ->execute();
  $hosts_suponly_checked = array();
  if (isset($akt_result['node'])) {
    foreach ($akt_result['node'] as $akt_stub) {
      $akt_host_smid = srr_core_node_get_single_field_value($akt_stub->extraFields->field_akt_szi_siarep_target_id, 'field_smid', 'value');      
      if ($akt_host_smid && !isset($hosts_suponly_checked[$akt_host_smid])) {
        $host_nid = srr_inventagent_get_host_by_smid($akt_host_smid);
//        if (!$host_nid) {
//          continue;
//        }
//        $host_name = srr_core_node_get_single_field_value($akt_host_nid, 'field_host_name', 'value');
//        if (isset($hosts[$host_name])) {
//          continue;
//        }
        $akt = entity_load_single('node', $akt_stub->nid); 
        $host_info = array();
        $host_info['account_name'] = 'недоменный';
        $host_info['owner_fio'] = $akt->field_akt_szi_user[LANGUAGE_NONE][0]['value'];
        $szi = drupal_json_decode($akt->field_akt_szi_presented[LANGUAGE_NONE][0]['value'], TRUE);
        if (is_array($szi)) {
          foreach ($szi as $single_szi) {
            $display = sup_sziflow_display_szi($single_szi);
            switch ($single_szi['type']) {
              case 'sn':
              case 'cpcsp':
              case 'crarm':
                $host_info['inv_'. $single_szi['type']] = $display['display_name'];
                $host_info['inv_'.$single_szi['type'].'_sn'] = $display['license'];
                break; 
              case 'vn':
                $host_info['inv_vn'] = $display['display_name'];
                $host_info['inv_vnap_id'] = drupal_strtolower($single_szi['_vn__apid']);
//                $host_info['inv_vnap_name'] = sup_sziflow_get_vnap_name_by_id($single_szi['_vn__apid']);
                break;
            }
          }
        }
//        if (isset($host_info['inv_vnap_id'])) {
          $same_host = FALSE;
          foreach ($hosts as $k => $host) {
            if (isset($host_info['inv_vnap_id']) && !empty($host['inv_vnap_id']) && $host['inv_vnap_id'] == $host_info['inv_vnap_id']) {
              $same_host = $k;
              $host_info = $host;
              break;
            } 
            elseif (!empty($host['owner_fio']) && $host['owner_fio'] == $host_info['owner_fio']) {
              $same_host = $k;
              $host_info = $host;
              break;
            }
          }
          if ($same_host) {
          } elseif (!empty($host['inv_vnap_id'])) {
            $host_info['inv_vnap_name'] = sup_sziflow_get_vnap_name_by_id($host['inv_vnap_id']);
          }
//        }
        $host_info['inv_no'] = $akt->field_akt_szi_inv[LANGUAGE_NONE][0]['value'];
//        $host_info['owner_fio'] = empty($host_info['owner_fio']) ? $akt->field_akt_szi_user[LANGUAGE_NONE][0]['value'] : $host_info['owner_fio'];
        $host_info['addr'] = $akt->field_akt_szi_host_addr[LANGUAGE_NONE][0]['value'];
        $host_info['akt_szi'] = array(sprintf('%s (%s)', $akt_stub->extraFields->title, date('d.m.Y', $akt_stub->extraFields->field_akt_szi_date_value)));
/*
            [account_name] => nad-ws0014
            [activity] => 1536934030
            [sinv_time] => 1536930113
            [inv_sn] => Secret Net 7.6.604.0 (сетевой)
            [inv_sn_sn] => TAMV07KFBUJ200OG0FVP00P2000D
            [inv_vn] => ViPNet Client 4.3.2.37273
            [inv_cpcsp] => КриптоПро CSP 3.6.7777
            [inv_cpcsp_sn] => 36365Y000001HCUHU2FPYTWCR
            [inv_cpcsp_install] => 1520730000
            [inv_vnap_name] => КС_Служба РК по тех надзору_МуртазинаИБ_QSK-111
            [inv_vnap_id] => 06390196
            [owner_fio] => Микова Ирина Васильевна (miv003@rk.local)

*/              
        $k = !$same_host ? 'not_domain_'. $akt_host_smid : $same_host; 
        $hosts[$k] = $host_info;
        $hosts_suponly_checked[$akt_host_smid] = $k;
      } else {
        $hosts[$hosts_suponly_checked[$akt_host_smid]]['akt_szi'][] = sprintf('%s (%s)', $akt_stub->extraFields->title, date('d.m.Y', $akt_stub->extraFields->field_akt_szi_date_value));
      }
    }
  }

  $fios = array();
  foreach ($hosts as $k => $host) { 
    if (!empty($host['owner_fio'])) {
      if (!isset($fios[$host['owner_fio']])) {
        $fios[$host['owner_fio']] = array();
      }
      $fios[$host['owner_fio']][] = $k;
    }
  }
  if (!empty($fios)) {
    $acc_query = new EntityFieldQueryExtraFields();
    $acc_result = $acc_query
      ->entityCondition('entity_type', 'user')
      ->fieldCondition('field_org_raw', 'target_id', $org_nid)
      ->fieldCondition('field_user_fio', 'value', array_keys($fios), 'IN')
      ->addExtraField('field_user_fio', 'value', 'value')
      ->execute();
    if (isset($acc_result['user'])) {
      $quiz = NULL;
      foreach ($acc_result['user'] as $acc_stub) {
        $fio = $acc_stub->extraFields->field_user_fio_value;
        $result = db_query(
          'SELECT qnp.nid, qnp.vid, qnrs.*
           FROM {quiz_node_properties} qnp
           INNER JOIN {quiz_node_results} qnrs ON qnrs.vid = qnp.vid
           WHERE qnrs.uid=:uid AND qnp.nid = :nid AND qnrs.is_evaluated=1
           ORDER BY qnrs.score DESC, qnrs.time_start DESC
           LIMIT 1', array(':uid' => $acc_stub->uid, ':nid' => 3152))->fetch();
        $pass = FALSE;
        if (is_object($result)) {
          $quiz = !$quiz ? node_load($result->nid, $result->vid) : $quiz;
          $questions = _quiz_get_answers($quiz, $result->result_id);
          $score = quiz_calculate_score($quiz, $result->result_id);
          $pass = $result->score >= $quiz->pass_rate;
        }
        foreach ($fios[$fio] as $host_k) {
          $hosts[$host_k]['skzitest_pass'] = $pass;
        }
      }
    }
  }

  return $hosts;
//  print_r ($hosts);
}


function sup_sziflow_get_vnap_name_by_id($vnap_id) {
  $vnap_id_lc = drupal_strtolower($vnap_id);

  $url = 'http://secmon.rk.local/api/v1.0/vipnet-apinfo/'. $vnap_id_lc .'/'.
    '?token=KwO07O02OXRM7Pga6RersYUrO55Pvt_WNrsSEnmxpT8';
  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
  $api_result = curl_exec($ch);
  $api_result = drupal_json_decode($api_result);
  $api_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  if ($api_code == 200) {
    return $api_result['data']['info'][$vnap_id_lc]['title'];
  } else {
    return FALSE;
  }
}



function sup_sziflow_skzicab_get_hosts_from_secmon($org_name) {
  $api_token = variable_get('sup_secmon_api_token');
  $api_url = variable_get('sup_sziflow_skzicab_org_hosts_info_api_url');
  $api_url .= '?org_name='. urlencode($org_name);

  $ch = curl_init();
  curl_setopt($ch, CURLOPT_URL, $api_url);
  curl_setopt($ch, CURLOPT_HTTPHEADER, ['access-token: ' . $api_token]);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
  curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
  $response = curl_exec($ch);
  curl_close($ch);

  $response = drupal_json_decode($response, TRUE);

  return $response ? $response['data']['hosts'] : array();
}


function sup_sziflow_skzicab_hosts($org) {
  $output = '';

  $header = array('Компьютер', 'СЗИ', 'Документы', 'Тест СКЗИ');
  $rows = array(); 
  $hosts = sup_sziflow_tmp_skzicab_hoststest();
//  dpm($hosts);
  @uasort($hosts, 'sup_sziflow_hosts_sort_by_owner_fio');
  
  foreach ($hosts as $host) {
    $row = array();
    $fio = !empty($host['owner_fio']) ? check_plain($host['owner_fio']) : '-';
    $p = array();
    $p[] = '<b>'. $fio .'</b>';
    $p[] = !empty($host['account_name']) ? ('<small>'. $host['account_name'] .'</small>') : '';
    $p[] = !empty($host['inv_no']) ? ('<small>'. $host['inv_no'] .'</small>') : '';
    $p[] = !empty($host['addr']) ? ('<small>'. $host['addr'] .'</small>') : '';
    $p[] = !empty($host['sinv_time']) ? ('<small>Инв. по домену: '. date('d.m.Y', $host['sinv_time']) .'</small>') : '';
    $p[] = !empty($host['activity']) ? ('<small>Включался: '. date('d.m.Y', $host['activity']) .'</small>') : '';
    $p = array_filter($p);
    $row[] = implode('<br/>', $p);

    $p = array();
    $p[] = !empty($host['inv_sn']) ? ($host['inv_sn'] .' - '. $host['inv_sn_sn']) : '';
    $p[] = !empty($host['inv_vn']) ? ($host['inv_vn'] .' - '. $host['inv_vnap_id'] .' - '. $host['inv_vnap_name']) : '';
    $p[] = !empty($host['inv_cpcsp']) ? ($host['inv_cpcsp'] .' - '. $host['inv_cpcsp_sn']) : '';
    $p[] = !empty($host['inv_crarm']) ? ($host['inv_crarm'] .' - '. $host['inv_crarm_sn']) : '';
    $p = array_filter($p);
    $row[] = implode('<br/>', $p);

    $akt_szi = '';    
    if (!empty($host['akt_szi'])) {
      $akt_szi = implode('<br/>', $host['akt_szi']);
      $akt_szi = l($akt_szi, 'node/'. $org->nid .'/skzicab', array('query' => array('search' => $host['owner_fio']), 'html' => TRUE));
    }   
    $row[] = $akt_szi;
    $row[] = l((isset($host['skzitest_pass']) && $host['skzitest_pass']) ? 'Тест пройден' : 'Тест не пройден', 'node/'. $org->nid .'/skzicab/test', array('query' => array('result_type' => 1, 'fio' => $host['owner_fio'])));
    $rows[] = $row;
  }

  $output .= theme('table', array('header' => $header, 'rows' => $rows));
  return $output;
}


function sup_sziflow_hosts_sort_by_owner_fio($a, $b) {
  return strnatcmp($a['owner_fio'], $b['owner_fio']);
}



