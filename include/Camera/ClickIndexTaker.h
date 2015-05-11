#include "Image.h"
#include "IndexTaker.h"

namespace Camera{
  class ClickIndexTaker : public IndexTaker {
    private:
      Image _data;
    public:
      ClickIndexTaker(const Image& data);
      virtual void updateCoords(int row, int column);
  };
}

