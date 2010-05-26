<?php
// $Id$

/**
 * @file extension to imageapi, provide an overlay action for blending two
 * layers, preserving transparency.
 */

// Not sure where this library will live between upgrade versions.
// Be careful to not conflict with another copy of myself.
if (! function_exists('imageapi_image_overlay')) {

/**
 * Place one image over another
 *
 * @param $image 
 *   Base imageapi object.
 * @param $overlay 
 *   May be a filename or an imageAPI object
 * @param $x 
 *   Position of the overlay
 * @param $y 
 *   Position of the overlay
 * @param $alpha 
 *   Transparency of the overlay from 0-100. 0 is totally transparent. 100
 * (default) is totally opaque.
 * @param $reverse 
 *   BOOL flag to indicate the 'overlay' actually goes under the image. As
 * the imageapi callbacks modify the $image object by reference, this is needed
 * to replace the old image resource with the new one.
 * @return bool success
 * 
 * @ingroup imageapi
 */
function imageapi_image_overlay(&$image, &$layer, $x, $y, $alpha = 100, $reverse = FALSE) {
  if (is_string($layer) ) {
    if (! file_exists($layer)) {
      trigger_error("Image file does not exist. Attempted to overlay $layer", E_USER_ERROR);
      return FALSE;
    }
    $layer = imageapi_image_open($layer);
  }
  // else $layer had better be an image handle

  $x = imagecache_actions_keyword_filter($x, $image->info['width'], $layer->info['width']);
  $y = imagecache_actions_keyword_filter($y, $image->info['height'], $layer->info['height']);

  return imageapi_toolkit_invoke('overlay', $image, array($layer, $x, $y, $alpha, $reverse));
}

/**
 * Place one image over another
 * This modifies the passed image by reference
 * 
 * This func is nominated for inclusion in imageapi package. Until then, we do
 * it ourselves.
 * 
 * NOTE that the PHP libraries are not great at merging images SO we include a
 * library that does it pixel-by-pixel which is INCREDIBLY inefficient. If this
 * can be improved, in a way that supports all transparency, please let us know!
 *
 * A watermark is layer onto image, return the image. An underlay is image onto
 * layer, return the layer. Almost identical, but seeing as we work with
 * resource handles, the handle needs to be swapped before returning.
 *
 * @ingroup imageapi
 * @param $image 
 *   Base imageapi object.
 * @param $overlay 
 *   May be a filename or an imageAPI object
 * @param $x 
 *   Position of the overlay
 * @param $y 
 *   Position of the overlay
 * @param $alpha 
 *   Transparency of the overlay from 0-100. 0 is totally transparent. 100
 * (default) is totally opaque.
 * @param $reverse 
 *   BOOL flag to indicate the 'overlay' actually goes under the image. As
 * the imageapi callbacks modify the $image object by reference, this is needed
 * to replace the old image resource with the new one.
 * @return bool success
 */
function imageapi_gd_image_overlay(&$image, &$layer, $x, $y, $alpha = 100, $reverse = FALSE) {
  if (empty($layer->resource)) {
    trigger_error("Invalid input to ". __FUNCTION__ . " 'layer' is not a valid resource");
    #dpm($layer);
    return FALSE;
  }
  // If the given alpha is 100%, we can use imagecopy - which actually works, 
  // Is more efficient, and seems to retain the overlays partial transparancy
  // Still does not work great for indexed gifs though?

  if ($alpha == 100 && ($layer->info['mime_type'] != 'image/gif')) {
    imagealphablending($image->resource, TRUE);
    imagesavealpha($image->resource, TRUE);
    imagealphablending($layer->resource, TRUE);
    imagesavealpha($layer->resource, TRUE);
    imagecopy($image->resource, $layer->resource, $x, $y, 0, 0, $layer->info['width'], $layer->info['height']);
    imagedestroy($layer->resource);
    #imagealphablending($image->resource, FALSE);
  }
  else {
    // Else imagecopymerge fails and we have to use the slow library 
    require_once('watermark.inc');
    $watermark = new watermark();
    $image->resource = $watermark->create_watermark($image->resource, $layer->resource, $x, $y, $alpha);
    imagedestroy($layer->resource);
  }

  if ($reverse) {
    // When doing underlay, It's the second image object that we really care about. 
    // Update that with the result
    $layer->resource = $image->resource;
    $layer->info = $image->info;
  }
  return TRUE;
}

/**
 * Improvements on this are welcomed!
 * 
 * Please be aware of the limitations of imagemagick libraries out there - the
 * versions distributed on hosted servers (if any) are often several years
 * behind. Using the latest imagemagick release features will make this function
 * unusable in real deployments.
 * 
 */
function imageapi_imagemagick_image_overlay(&$image, &$layer, $x = 0, $y = 0, $alpha = 100, $reverse = FALSE) {
  $layer_filepath = $layer->source;
  # TODO - alpha channels
  # I spent ages on the docs, but it appears my version of convert does not support -merge, -watermark or -dissolve
  // Bloody libraries - I tried  [6.2.8 06/11/08] because thats what I could get for my distro.
  
  # This just drops the image on, no alpha:
  if ($alpha == 100) {
    $image->ops[] = "  \"$layer_filepath\" -geometry +$x+$y  -composite    "  ;
  }
  else {
    $compose_arg = "  ";
    # $compose_arg = " -compose dissolve ";
    // -compose disolve is supposed to work, but doesn't in available imagemagick versions
    $geometry_arg = " -geometry +$x+$y ";
    $alpha_arg = " -set \"option:compose:args\" $alpha ";
    $image->ops[] = "  \"$layer_filepath\" $compose_arg $geometry_arg $alpha_arg  -composite ";
 
  }
  #  watchdog('imagecache_canvas', print_r($image->ops, 1) );          
  # This also worked
  #  $image->ops[] = ' -draw "image over {$x},{$y} 0,0 \'{$layer_filepath}\'"'  ;

  // TODO - I may end up with a different sized image from doing this?
  // How to let the object know?

  return TRUE;
}

}