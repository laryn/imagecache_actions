<?php
/**
 * Utility form, conversion and rendering functions for image processes
 */



/**
 * Prepare a subform for displaying RGB fields
 *
 * Helper function to render a common element.
 * 
 * Note that any module that re-uses this form also has to declare the theme
 * function in order to ensure it's in the registry.
 */
function imagecache_rgb_form($action) {

  if($action['HEX'] && $deduced = hex_to_rgb($action['HEX'])) {
    $action = array_merge($action, $deduced);
    $action['HEX'] = ltrim($action['HEX'], '#');
    // With or without # is valid, but trim for consistancy
  }
  $form = array('#theme' => 'imagecacheactions_rgb_form'); 
  $form['farb'] = array('#weight' => -1); // Placeholder to get its weight right
  $form['HEX'] = array( '#type' => 'textfield', '#title' => t('HEX'), '#default_value' => $action['HEX'], '#size' => 7);

  return $form;
}


/**
 * Prepare a subform for displaying positioning fields
 *
 * Helper function to render a common element.
 */
function imagecacheactions_pos_form($action) {
  $defaults = array(
    'xpos' => 'center',
    'ypos' => 'center',
  );
  $action = array_merge($defaults, (array)$action);

  $form = array(
    #'#theme' => 'canvasactions_pos_form',
    'xpos' => array(
      '#type' => 'textfield',
      '#title' => t('X offset'),
      '#default_value' => $action['xpos'],
      '#size' => 6,
      '#description' => t('Enter an offset in pixels or use a keyword: <em>left</em>, <em>center</em>, or <em>right</em>.'),
      '#element_validate' => array('imagecache_actions_validate_number'),
    ),
    'ypos' => array(
      '#type' => 'textfield',
      '#title' => t('Y offset'),
      '#default_value' => $action['ypos'],
      '#size' => 6,
      '#description' => t('Enter an offset in pixels or use a keyword: <em>top</em>, <em>center</em>, or <em>bottom</em>.'),
      '#element_validate' => array('imagecache_actions_validate_number'),
    ),
  );
  return $form;
}

/**
 * Ensure the numbers are valid.
 * 
 * Set blanks to zero, just so the status summary doesn't get odd blanks
 */
function imagecache_actions_validate_number(&$element, &$form_state) {
  if(empty($element['value'])) form_set_value($element, 0, $form_state);
}

function imagecache_actions_validate_alpha(&$element, &$form_status) {
  if (!is_numeric($element['#value']) || $element['#value'] < 1 || $element['#value'] > 100) {
    form_set_error(join('][', $element['#parents']), t('Opacity must be a number between 1 and 100.'));
  }
}


/**
 * Render the subform in a table
 */
function theme_imagecacheactions_rgb_form(&$form) {

  // Add a farb element
  drupal_add_css('misc/farbtastic/farbtastic.css', 'module', 'all', FALSE);
  drupal_add_js('misc/farbtastic/farbtastic.js');
//  drupal_add_js(drupal_get_path('module', 'imagecache_coloractions') . '/color.js');

  $hex_id = $form['HEX']['#id'];
  $form['farb'] = array('#value' => "<div id=\"$hex_id-farb\" style=\"float:right\"></div>", '#weight' => -1 );

  // Adds the JS that binds the textarea with the farb element
  $js = "
  $(document).ready(function() {
    farbify($('#$hex_id'), '#$hex_id-farb');
  });

  function farbify(elt, wrapper) {
    var farb = $.farbtastic(wrapper);
    farb.linkTo(function(color) {
        elt
          .css('background-color', color)
          .css('color', this.hsl[2] > 0.5 ? '#000' : '#fff')
          .val(color.substring(1));
      });
    farb.setColor('#' + elt.val());
    elt.bind('keyup', function(){ updateColor(elt, farb); });
  }
  function updateColor(elt, farb) {
    var text = elt.val();
    if (text.length == 6)
      farb.setColor('#' + text);
  }

  ";
  drupal_add_js($js, 'inline');  
  $output = drupal_render($form);
  return $output;
}


function theme_imagecacheactions_rgb($rgb) {
  if ($rgb['HEX']) {
    return " <span style=\"width:2em; border:1px solid white; background-color:#{$rgb["HEX"]}\" >&nbsp;#{$rgb['HEX']}&nbsp;</span>";
  }
  else {
    return ' ' .t('Transparent');
  }
}



 /**
 * Decode an HTML hex-code into an array of R, G, and B values.
 * accepts these formats: (case insensitive) #ffffff, ffffff, #fff, fff
 */
function hex_to_rgb($hex) {
  $hex = trim($hex);
  // remove '#'
  if (substr($hex, 0, 1) == '#')
    $hex = substr($hex, 1) ;

  // expand short form ('fff') color
  if (strlen($hex) == 3) {
    $hex = substr($hex, 0, 1) . substr($hex, 0, 1) .
           substr($hex, 1, 1) . substr($hex, 1, 1) .
           substr($hex, 2, 1) . substr($hex, 2, 1) ;
  }

  if (strlen($hex) != 6)
    trigger_error('Error: Invalid color "'. $hex .'"') ;

  // convert
  $rgb['red'] = hexdec(substr($hex, 0, 2)) ;
  $rgb['green'] = hexdec(substr($hex, 2, 2)) ;
  $rgb['blue'] = hexdec(substr($hex, 4, 2)) ;

  return $rgb ;
}

/**
 * Accept a keyword (center, top, left, etc) and return it as an offset in pixels.
 * Called on either the x or y values.
 * 
 * May  be something like "20", "center", "left+20", "bottom+10". + values are
 * in from the sides, so bottom+10 is 10 UP from the bottom. 
 * 
 * "center+50" is also OK.
 * 
 * "30%" will place the CENTER of the object at 30% across. to get a 30% margin,
 * use "left+30%"
 * 
 * @param $value 
 *   string or int value. 
 * @param $current_size
 *   int size in pixels of the range this item is to be placed in
 * @param $object_size
 *   int size in pixels of the object to be placed
 * 
 * 
 */
function imagecache_actions_keyword_filter($value, $current_size, $object_size) {
  $keyword = 0; 
  $offset = 0;
  
  // Check if we have plus or minus values
  $keyword_split = explode('+', $value);
  $keyword = $keyword_split[0];
  if (! empty($keyword_split[1]) ) {
    $offset = $keyword_split[1];
  }
  if (strstr($value, '-')) {
    $k2 = explode('-', $value);
    $keyword = $k2[0];
    $offset = -intval($k2[1]);
  }

  // handle % values
  if (substr($value, strlen($value), 1) == '%') {
    $percent = TRUE;
    $offset = intval($offset);
    $value = intval($value / 100 * $current_size);
    if (! $keyword) {
      // just said eg '25%' - so center the object there
      $offset = $object_size / -2;
    } 
    else {
      // Otherwise the keyword will give us the new value, and the % will be its offset
      $offset = intval($offset / 100 * $current_size);
    }
  }


  // Whether the value is 'top or left doesn't matter, treat both axes the same.
  switch ($keyword) {
    case 'top':
    case 'left':
      $value = 0;
      break;
    case 'bottom':
    case 'right':
      $value = $current_size - $object_size;
      $offset = -1 * $offset;
      break;
    case 'center':
      $value = $current_size/2 - $object_size/2;
      break;
    default :
      // if no keyword, it was a raw number - assume top left then
      $value = intval($value);
  }
  #dpm("Placing an object $object_size big on a range of $current_size at a position of $value , $offset");  
  // Add any extra negative or positive
  if (!empty($offset)) {
    $value = $value + $offset;
  }
  return $value;
}


/**
 * imagecache is conservative with its inclusion of inc files, but sometimes I
 * need to use them - eg crop. This function finds and includes it if needed.
 */
function imagecache_include_standard_actions() {
  $cropaction = imagecache_action_definition('imagecache_crop');
  include_once($cropaction['file']);
}