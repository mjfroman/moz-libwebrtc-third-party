# Copyright 2022 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Question answers protobuf."""

import dataclasses
from typing import Any, List

from tensorflow_lite_support.cc.task.processor.proto import qa_answers_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls

_PosProto = qa_answers_pb2.QaAnswer.Pos
_QaAnswerProto = qa_answers_pb2.QaAnswer
_QuestionAnswererResultProto = qa_answers_pb2.QuestionAnswererResult


@dataclasses.dataclass
class Pos:
  """Position information of the answer relative to context.

  It is sortable in
  descending order based on logit.

  Attributes:
    start: The start offset index of the answer.
    end: The end offset index of the answer.
    logit: The logit of the answer. Higher value means it's more likely to be
      the answer.
  """

  start: int
  end: int
  logit: float

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _PosProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _PosProto(start=self.start, end=self.end, logit=self.logit)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _PosProto) -> "Pos":
    """Creates a `Pos` object from the given protobuf object."""
    return Pos(start=pb2_obj.start, end=pb2_obj.end, logit=pb2_obj.logit)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, Pos):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class QaAnswer:
  """Represents the Answer to BertQuestionAnswerer.

  Attributes:
    pos: The relative position of the answer in the context.
    text: The answer text.
  """

  pos: Pos
  text: str

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _QaAnswerProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _QaAnswerProto(pos=self.pos.to_pb2(), text=self.text)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _QaAnswerProto) -> "QaAnswer":
    """Creates a `QaAnswer` object from the given protobuf object."""
    return QaAnswer(pos=Pos.create_from_pb2(pb2_obj.pos), text=pb2_obj.text)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, QaAnswer):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class QuestionAnswererResult:
  """The list of probable answers generated by BertQuestionAnswerer.

  Attributes:
    answers: A list of `QaAnswer` objects.
  """

  answers: List[QaAnswer]

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _QuestionAnswererResultProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _QuestionAnswererResultProto(
        answers=[answer.to_pb2() for answer in self.answers])

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(
      cls, pb2_obj: _QuestionAnswererResultProto) -> "QuestionAnswererResult":
    """Creates a `QuestionAnswererResult` object from the given protobuf object."""
    return QuestionAnswererResult(answers=[
        QaAnswer.create_from_pb2(answer) for answer in pb2_obj.answers
    ])

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, QuestionAnswererResult):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
