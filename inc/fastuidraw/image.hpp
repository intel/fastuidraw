/*!
 * \file image.hpp
 * \brief file image.hpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */



#pragma once

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{

///@cond
class Image;
///@endcond

/*!\addtogroup Imaging
 * @{
 */

  /*!
   * \brief
   * ImageSourceBase defines the inteface for copying texel data
   * from a source (CPU memory, a file, etc) to an
   * AtlasColorBackingStoreBase derived object.
   */
  class ImageSourceBase
  {
  public:
    virtual
    ~ImageSourceBase()
    {}

    /*!
     * To be implemented by a derived class to return true
     * if a region (across all mipmap levels) has a constant
     * color.
     * \param location location at LOD 0
     * \param square_size width and height of region to check
     * \param dst if all have texels of the region have the same
     *            color, write that color value to dst.
     */
    virtual
    bool
    all_same_color(ivec2 location, int square_size, u8vec4 *dst) const = 0;

    /*!
     * To be implemented by a derived class to return the number of
     * mipmap levels of image source has.
     */
    virtual
    unsigned int
    num_mipmap_levels(void) const = 0;

    /*!
     * To be implemented by a derived class to write to
     * a \ref c_array of \ref u8vec4 data a rectangle of
     * texels of a particular mipmap level. NOTE: if pixels
     * are requested to be fetched from outside the sources
     * natural dimensions, those pixels outside are to be
     * duplicates of the boundary values.
     *
     * \param mimpap_level LOD of data where 0 represents the highest
     *                     level of detail and each successive level
     *                     is half the resolution in each dimension;
     *                     it is gauranteed by the called that
     *                     0 <= mimpap_level() < num_mipmap_levels()
     * \param location (x, y) location of data from which to copy
     * \param w width of data from which to copy
     * \param h height of data from which to copy
     * \param dst location to which to write data, data is to be packed
     *            dst[x + w * y] holds the texel data
     *            (x + location.x(), y + location.y()) with
     *            0 <= x < w and 0 <= y < h
     */
    virtual
    void
    fetch_texels(unsigned int mimpap_level, ivec2 location,
                 unsigned int w, unsigned int h,
                 c_array<u8vec4> dst) const = 0;
  };

  /*!
   * \brief
   * An implementation of \ref ImageSourceBase where the data is backed
   * by a c_array<const u8vec4> data.
   */
  class ImageSourceCArray:public ImageSourceBase
  {
  public:
    /*!
     * Ctor.
     * \param dimensions width and height of the LOD level 0 mipmap;
     *                   the LOD level n is then assumed to be size
     *                   (dimensions.x() >> n, dimensions.y() >> n)
     * \param pdata the texel data, the data is NOT copied, thus the
     *              contents backing the texel data must not be freed
     *              until the ImageSourceCArray goes out of scope.
     */
    ImageSourceCArray(uvec2 dimensions,
                      c_array<const c_array<const u8vec4> > pdata);
    virtual
    bool
    all_same_color(ivec2 location, int square_size, u8vec4 *dst) const;

    virtual
    unsigned int
    num_mipmap_levels(void) const;

    virtual
    void
    fetch_texels(unsigned int mimpap_level, ivec2 location,
                 unsigned int w, unsigned int h,
                 c_array<u8vec4> dst) const;

  private:
    uvec2 m_dimensions;
    c_array<const c_array<const u8vec4> > m_data;
  };

  /*!
   * \brief
   * Represents the interface for a backing store for color data of images.
   *
   * For example in GL, this can be a GL_TEXTURE_2D_ARRAY. An implementation
   * of the class does NOT need to be thread safe because the user of the
   * backing store (ImageAtlas) performs calls to the backing store behind
   * its own mutex.
   */
  class AtlasColorBackingStoreBase:
    public reference_counted<AtlasColorBackingStoreBase>::default_base
  {
  public:
    /*!
     * Ctor.
     * \param whl provides the dimensions of the AtlasColorBackingStoreBase
     * \param presizable if true the object can be resized to be larger
     */
    AtlasColorBackingStoreBase(ivec3 whl, bool presizable);

    /*!
     * Ctor.
     * \param w width of the backing store
     * \param h height of the backing store
     * \param num_layers number of layers of the backing store
     * \param presizable if true the object can be resized to be larger
     */
    AtlasColorBackingStoreBase(int w, int h, int num_layers, bool presizable);

    virtual
    ~AtlasColorBackingStoreBase();

    /*!
     * To be implemented by a derived class to set color data into the backing store.
     * \param mimap_level what mipmap level
     * \param dst_xy x and y coordinates of location to place data in the atlas
     * \param dst_l layer of position to place data in the atlas
     * \param src_xy x and y coordinates from which to take data from the ImageSourceBase
     * \param size width and height of region to copy into the backing store.
     * \param data ImageSourceBase from which to src data
     */
    virtual
    void
    set_data(int mimap_level, ivec2 dst_xy, int dst_l, ivec2 src_xy,
             unsigned int size, const ImageSourceBase &data) = 0;

    /*!
     * To be implemented by a derived class to set color data into the backing store.
     * \param mimap_level what mipmap level
     * \param dst_xy x and y coordinates of location to place data in the atlas
     * \param dst_l layer of position to place data in the atlas
     * \param size width and height of region to copy into the backing store.
     * \param color_value value with which to fill EVERY texel of the
     */
    virtual
    void
    set_data(int mimap_level, ivec2 dst_xy, int dst_l, unsigned int size, u8vec4 color_value) = 0;

    /*!
     * To be implemented by a derived class
     * to flush set_data() to the backing
     * store.
     */
    virtual
    void
    flush(void) = 0;

    /*!
     * Returns the dimensions of the backing store
     * (as passed in the ctor).
     */
    ivec3
    dimensions(void) const;

    /*!
     * Returns true if and only if this object can be
     * resized to a larger size.
     */
    bool
    resizeable(void) const;

    /*!
     * Resize the object by increasing the number of layers.
     * The routine resizeable() must return true, if not
     * the function FASTUIDRAWasserts.
     */
    void
    resize(int new_num_layers);

  protected:
    /*!
     * To be implemented by a derived class to resize the
     * object. The resize changes ONLY the number of layers
     * of the object and only increases the value as well.
     * When called, the return value of dimensions() is
     * the size before the resize completes.
     * \param new_num_layers new number of layers to which
     *                       to resize the underlying store.
     */
    virtual
    void
    resize_implement(int new_num_layers) = 0;

  private:
    void *m_d;
  };

  /*!
   * \brief
   * Represents the interface for the backing store for index data of images.
   *
   * For example in GL, this can be a GL_TEXTURE_2D_ARRAY. An implementation
   * of the class does NOT need to be thread safe because the user of the
   * backing store (ImageAtlas) performs calls to the backing store behind
   * its own mutex.
   */
  class AtlasIndexBackingStoreBase:
    public reference_counted<AtlasIndexBackingStoreBase>::default_base
  {
  public:
    /*!
     * Ctor.
     * \param whl dimensions of the backing store
     * \param presizable if true the object can be resized to be larger
     */
    AtlasIndexBackingStoreBase(ivec3 whl, bool presizable);

    /*!
     * Ctor.
     * \param w width of the backing store
     * \param h height of the backing store
     * \param l number layers of the backing store
     * \param presizable if true the object can be resized to be larger
     */
    AtlasIndexBackingStoreBase(int w, int h, int l, bool presizable);

    virtual
    ~AtlasIndexBackingStoreBase();

    /*!
     * To be implemented by a derived class to fill index data into the
     * backing store. The index data passed references into a
     * AtlasColorBackingStore.
     * \param x horizontal position
     * \param y vertical position
     * \param l layer position
     * \param w width of data
     * \param h height of data
     * \param data list of tiles as returned by ImageAtlas::add_color_tile()
     * \param slack amount of pixels duplicated on each boundary, in particular
     *              the actual color data "starts" slack pixels into the color
     *              tile and "ends" slack pixels before. The purpose of the
     *              slack is to allow to sample outside of the tile, for example
     *              to do bilinear filtering (requires slack of 1) or cubic
     *              filtering (require slack of 2). The slack value is to be
     *              used by an implementation to help compute the actaul texel
     *              values to store in the underlying texture.
     * \param c AtlasColorBackingStoreBase into which to index
     * \param color_tile_size size of tiles on C
     */
    virtual
    void
    set_data(int x, int y, int l,
             int w, int h,
             c_array<const ivec3> data,
             int slack,
             const AtlasColorBackingStoreBase *c,
             int color_tile_size) = 0;

    /*!
     * To be implemented by a derived class to fill index data into the
     * backing store. The index data passed references back into this
     * AtlasIndexBackingStore.
     * \param x horizontal position
     * \param y vertical position
     * \param l layer position
     * \param w width of data
     * \param h height of data
     * \param data list of tiles as returned by ImageAtlas::add_index_tile()
     */
    virtual
    void
    set_data(int x, int y, int l, int w, int h,
             c_array<const ivec3> data) = 0;

    /*!
     * To be implemented by a derived class
     * to flush set_data() to the backing
     * store.
     */
    virtual
    void
    flush(void) = 0;

    /*!
     * Returns the dimensions of the backing store
     * (as passed in the ctor).
     */
    ivec3
    dimensions(void) const;

    /*!
     * Returns true if and only if this object can be
     * resized to a larger size.
     */
    bool
    resizeable(void) const;

    /*!
     * Resize the object by increasing the number of layers.
     * The routine resizeable() must return true, if not
     * the function FASTUIDRAWasserts.
     */
    void
    resize(int new_num_layers);

  protected:
    /*!
     * To be implemented by a derived class to resize the
     * object. The resize changes ONLY the number of layers
     * of the object and only increases the value as well.
     * When called, the return value of dimensions() is
     * the size before the resize completes.
     * \param new_num_layers new number of layers to which
     *                       to resize the underlying store.
     */
    virtual
    void
    resize_implement(int new_num_layers) = 0;

  private:
    void *m_d;
  };


  /*!
   * \brief
   * An ImageAtlas is a common location to place images of an application.
   *
   * Ideally, all images are placed into a single ImageAtlas (changes of
   * ImageAtlas force draw-call breaks). Methods of ImageAtlas are
   * thread safe, locked behind a mutex of the ImageAtlas.
   */
  class ImageAtlas:
    public reference_counted<ImageAtlas>::default_base
  {
  public:
    /*!
     * Class representing an action to execute when a bindless
     * image is deleted. The action is gauranteed to be executed
     * AFTER the 3D API is no longer using the resources that
     * back the image.
     */
    class ResourceReleaseAction:
      public reference_counted<ResourceReleaseAction>::default_base
    {
    public:
      /*!
       * To be implemented by a derived class to perform resource
       * release actions.
       */
      virtual
      void
      action(void) = 0;
    };

    /*!
     * Ctor.
     * \param pcolor_tile_size size of each color tile
     * \param pindex_tile_size size of each index tile
     * \param pcolor_store color data backing store for atlas, the width and
     *                     height of the backing store must be divisible by
     *                     pcolor_tile_size.
     * \param pindex_store index backing store for atlas, the width and
     *                     height of the backing store must be divisible by
     *                     pindex_tile_size.
     */
    ImageAtlas(int pcolor_tile_size, int pindex_tile_size,
               reference_counted_ptr<AtlasColorBackingStoreBase> pcolor_store,
               reference_counted_ptr<AtlasIndexBackingStoreBase> pindex_store);

    virtual
    ~ImageAtlas();

    /*!
     * Returns the size (in texels) used for the index tiles.
     */
    int
    index_tile_size(void) const;

    /*!
     * Returns the size (in texels) used for the color tiles.
     */
    int
    color_tile_size(void) const;

    /*!
     * Calls AtlasIndexBackingStoreBase::flush() on
     * the index backing store (see index_store())
     * and AtlasColorBackingStoreBase::flush() on
     * the color backing store (see color_store()).
     */
    void
    flush(void) const;

    /*!
     * Returns a handle to the backing store for the image data.
     */
    const reference_counted_ptr<const AtlasColorBackingStoreBase>&
    color_store(void) const;

    /*!
     * Returns a handle to the backing store for the index data.
     */
    const reference_counted_ptr<const AtlasIndexBackingStoreBase>&
    index_store(void) const;

    /*!
     * Returns true if and only if the backing stores of the atlas
     * can be increased in size.
     */
    bool
    resizeable(void) const;

    /*!
     * Increments an internal counter. If this internal
     * counter is greater than zero, then the reurning
     * of tiles to the free store for later use is
     * -delayed- until the counter reaches zero again
     * (see unlock_resources()). The use case is for
     * buffered painting where the GPU calls are delayed
     * for later (to batch commands) and an Image may go
     * out of scope before the GPU commands are sent to
     * the GPU. By delaying the return of an Image's
     * tiles to the freestore, the image data is valid
     * still for rendering.
     */
    void
    lock_resources(void);

    /*!
     * Decrements an internal counter. If this internal
     * counter reaches zero, those tiles from Image's
     * that were deleted while the counter was non-zero,
     * are then returned to the tile free store. See
     * lock_resources() for more details.
     */
    void
    unlock_resources(void);

    /*!
     * Returns the number of index color tiles that are available
     * in the atlas without resizing the AtlasIndexBackingStoreBase
     * of the ImageAtlas.
     */
    int
    number_free_index_tiles(void) const;

    /*!
     * Adds an index tile that indexes into color data
     * \param data array of tiles as returned by add_color_tile()
     * \param slack amount of pixels duplicated on each boundary, in particular
     *              the actual color data "starts" slack pixels into the color
     *              tile and "ends" slack pixels before. The purpose of the
     *              slack is to allow to sample outside of the tile, for example
     *              to do bilinear filtering (requires slack of 1) or cubic
     *              filtering (require slack of 2).
     */
    ivec3
    add_index_tile(c_array<const ivec3> data, int slack);

    /*!
     * Adds an index tile that indexes into the index data. This is needed
     * for large images where more than one level of index look up is
     * needed to access the image
     * \param data array of tiles as returned by add_index_tile()
     */
    ivec3
    add_index_tile_index_data(c_array<const ivec3> data);

    /*!
     * Mark a tile as free in the atlas
     * \param tile tile to free as returned by add_index_tile().
     */
    void
    delete_index_tile(ivec3 tile);

    /*!
     * Adds a tile to the atlas returning the location
     * (in pixels) of the tile in the backing store
     * of the atlas.
     * \param src_xy location from ImageSourceBase to take data
     * \param image_data image data to which to set the tile
     */
    ivec3
    add_color_tile(ivec2 src_xy, const ImageSourceBase &image_data);

    /*!
     * Adds a tile of a constant color to the atlas returning
     * the location (in pixels) of the tile in the backing store
     * of the atlas.
     * \param color_data color value to which to set all pixels of
     *                   the tile
     */
    ivec3
    add_color_tile(u8vec4 color_data);

    /*!
     * Mark a tile as free in the atlas
     * \param tile tile to free as returned by add_color_tile().
     */
    void
    delete_color_tile(ivec3 tile);

    /*!
     * Returns the number of free color tiles that are available
     * in the atlas without resizing the AtlasColorBackingStoreBase
     * of the ImageAtlas.
     */
    int
    number_free_color_tiles(void) const;

    /*!
     * Resize the color and image backing stores so that
     * the given number of color and index tiles can also
     * be added to the atlas without needing to free any
     * tiles.
     * \param num_color_tiles number of color tiles
     * \param num_index_tiles number of index tiles
     */
    void
    resize_to_fit(int num_color_tiles, int num_index_tiles);

    /*!
     * Queue a ResourceReleaseAction to be executed when resources are
     * not locked down, see lock_resources() and unlock_resources().
     */
    void
    queue_resource_release_action(const reference_counted_ptr<ResourceReleaseAction> &action);

  private:
    void *m_d;
  };

  /*!
   * \brief
   * An Image represents an image comprising of RGBA8 values.
   * The texel values themselves are stored in a ImageAtlas.
   */
  class Image:
    public reference_counted<Image>::default_base
  {
  public:
    /*!
     * Gives the image-type of an Image
     */
    enum type_t
      {
        /*!
         * Indicates that the Image is on an ImageAtlas
         */
        on_atlas,

        /*!
         * Indicates that the Image is backed by a gfx API
         * texture via a bindless interface.
         */
        bindless_texture2d,

        /*!
         * Indicates to source the Image data from a currently
         * bound texture of the 3D API context. Using images
         * of these types should be avoided at all costs since
         * whenever one uses Image objects of these types
         * the 3D API state needs to change which induces a
         * stage change and draw break, harming performance.
         * The creation of these image types is to be handled
         * by the backend implementation.
         */
        context_texture2d,
      };

    /*!
     * Construct an \ref Image backed by an \ref ImageAtlas. If there is
     * insufficient room on the atlas, returns a nullptr handle.
     * \param atlas ImageAtlas atlas onto which to place the image.
     * \param w width of the image
     * \param h height of the image
     * \param image_data image data to which to initialize the image
     * \param pslack number of pixels allowed to sample outside of color tile
     *               for the image. A value of one allows for bilinear
     *               filtering and a value of two allows for cubic filtering.
     */
    static
    reference_counted_ptr<Image>
    create(const reference_counted_ptr<ImageAtlas> &atlas, int w, int h,
           const ImageSourceBase &image_data, unsigned int pslack);

    /*!
     * Construct an \ref Image backed by an \ref ImageAtlas. If there is
     * insufficient room on the atlas, returns a nullptr handle
     * \param atlas ImageAtlas atlas onto which to place the image
     * \param w width of the image
     * \param h height of the image
     * \param image_data image data to which to initialize the image
     * \param pslack number of pixels allowed to sample outside of color tile
     *               for the image. A value of one allows for bilinear
     *               filtering and a value of two allows for cubic filtering
     */
    static
    reference_counted_ptr<Image>
    create(const reference_counted_ptr<ImageAtlas> &atlas, int w, int h,
           c_array<const u8vec4> image_data, unsigned int pslack);

    /*!
     * Create an \ref Image backed by a bindless texture.
     * \param atlas ImageAtlas atlas onto which to place the image
     * \param w width of the image
     * \param h height of the image
     * \param m number of mipmap levels of the image
     * \param type the type of the bindless texture, must NOT have value
     *             \ref on_atlas.
     * \param handle the bindless handle value used by the Gfx API in
     *               shaders to reference the texture.
     * \param action action to call to release backing resources of
     *               the created Image.
     */
    static
    reference_counted_ptr<Image>
    create_bindless(const reference_counted_ptr<ImageAtlas> &atlas, int w, int h,
                    unsigned int m, enum type_t type, uint64_t handle,
                    const reference_counted_ptr<ImageAtlas::ResourceReleaseAction> &action =
                    reference_counted_ptr<ImageAtlas::ResourceReleaseAction>());

    ~Image();

    /*!
     * Returns the number of index look-ups to get to the image data.
     *
     * Only applies when type() returns \ref on_atlas.
     */
    unsigned int
    number_index_lookups(void) const;

    /*!
     * Returns the dimensions of the image, i.e the width and height.
     */
    ivec2
    dimensions(void) const;

    /*!
     * Returns the number of mipmap levels the image supports.
     */
    unsigned int
    number_mipmap_levels(void) const;

    /*!
     * Returns the slack of the image, i.e. how many texels ouside
     * of the image's sub-tiles from which one may sample.
     *
     * Only applies when type() returns \ref on_atlas.
     */
    unsigned int
    slack(void) const;

    /*!
     * Returns the "head" index tile as returned by
     * ImageAtlas::add_index_tile() or
     * ImageAtlas::add_index_tile_index_data().
     *
     * Only applies when type() returns \ref on_atlas.
     */
    ivec3
    master_index_tile(void) const;

    /*!
     * If number_index_lookups() > 0, returns the number of texels in
     * each dimension of the master index tile this Image lies.
     * If number_index_lookups() is 0, the returns the same value
     * as dimensions().
     *
     * Only applies when type() returns \ref on_atlas.
     */
    vec2
    master_index_tile_dims(void) const;

    /*!
     * Returns the quotient of dimensions() divided
     * by master_index_tile_dims().
     *
     * Only applies when type() returns \ref on_atlas.
     */
    float
    dimensions_index_divisor(void) const;

    /*!
     * Returns the ImageAtlas on which this Image resides.
     */
    const reference_counted_ptr<ImageAtlas>&
    atlas(void) const;

    /*!
     * Returns the bindless handle for the Image.
     *
     * Only applies when type() does NOT return \ref on_atlas.
     */
    uint64_t
    bindless_handle(void) const;

    /*!
     * Returns the image type.
     */
    type_t
    type(void) const;

  protected:
    /*!
     * Protected ctor for creating an Image backed by a bindless texture;
     * Applications should not use this directly and instead use create_bindless().
     * Backends should use this ctor for Image deried classes that do
     * cleanup (for example releasing the handle and/or deleting the texture).
     * \param atlas ImageAtlas atlas onto which to place the image.
     * \param w width of underlying texture
     * \param h height of underlying texture
     * \param m number of mipmap levels of the image
     * \param type the type of the bindless texture, must NOT have value
     *             \ref on_atlas.
     * \param handle the bindless handle value used by the Gfx API in
     *               shaders to reference the texture.
     * \param action action to call to release backing resources of
     *               the created Image.
     */
    Image(const reference_counted_ptr<ImageAtlas> &atlas, int w, int h,
          unsigned int m, enum type_t type, uint64_t handle,
          const reference_counted_ptr<ImageAtlas::ResourceReleaseAction> &action =
          reference_counted_ptr<ImageAtlas::ResourceReleaseAction>());

  private:
    Image(const reference_counted_ptr<ImageAtlas> &atlas, int w, int h,
          const ImageSourceBase &image_data, unsigned int pslack);

    void *m_d;
  };

/*! @} */

} //namespace
